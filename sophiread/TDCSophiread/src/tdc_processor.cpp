// TDCSophiread Section-Aware Processor Implementation
// Implements two-phase processing: TDC propagation + section processing

#include "tdc_processor.h"

#include <tbb/blocked_range.h>
#include <tbb/combinable.h>
#include <tbb/parallel_for.h>
#include <tbb/task_arena.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <execution>
#include <iostream>
#include <stdexcept>

#include "tdc_io.h"
#include "tdc_packet.h"

// HDF5 includes for streaming
#include <H5Cpp.h>

namespace tdcsophiread {

TDCProcessor::TDCProcessor(const DetectorConfig& config)
    : m_Config(config),
      m_MissingTdcCorrectionEnabled(config.isMissingTdcCorrectionEnabled()) {}

std::vector<TDCHit> TDCProcessor::processFile(const std::string& file_path,
                                              size_t chunk_size_mb,
                                              bool parallel,
                                              size_t num_threads) {
  auto start_time = std::chrono::high_resolution_clock::now();

  // Get total file size for chunking
  std::error_code ec;
  auto file_size = std::filesystem::file_size(file_path, ec);
  if (ec) {
    throw std::runtime_error("Cannot determine file size: " + file_path);
  }

  if (file_size == 0) {
    updateMetrics(std::chrono::microseconds(0), 0, 0);
    return {};
  }

  // Convert chunk size to bytes
  size_t chunk_size_bytes = chunk_size_mb * 1024 * 1024;

  // Reset TDC state for this file
  m_ChipTdcState = {0, 0, 0, 0};
  m_ChipHasTdc = {false, false, false, false};

  std::vector<TDCHit> all_hits;

  // Pre-allocate all_hits based on file size (Finding 1 optimization)
  // Using same 0.7 estimation factor as elsewhere in the code
  size_t file_total_packets = file_size / 8;
  size_t file_estimated_hits = static_cast<size_t>(file_total_packets * 0.7);
  all_hits.reserve(file_estimated_hits);

  size_t total_packets = 0;
  size_t current_offset = 0;

  // Process file in chunks
  while (current_offset < file_size) {
    // Determine chunk size (don't exceed file size)
    size_t remaining = file_size - current_offset;
    size_t current_chunk_size = std::min(chunk_size_bytes, remaining);

    // Map current chunk
    auto mapped_file =
        MappedFile::open(file_path, current_offset, current_chunk_size);

    // Find sections within this chunk
    auto chunk_sections =
        discoverSections(mapped_file->data(), mapped_file->size());

    if (chunk_sections.empty()) {
      // No sections found, advance to next chunk
      current_offset += current_chunk_size;
      continue;
    }

    // Adjust section offsets to be relative to file start
    for (auto& section : chunk_sections) {
      section.start_offset += current_offset;
      section.end_offset += current_offset;
    }

    // Apply "always leave last section" strategy (unless we're at end of file)
    std::vector<TDCSection> sections_to_process;
    bool at_end_of_file = (current_offset + current_chunk_size >= file_size);

    if (at_end_of_file) {
      // At end of file - process all sections
      sections_to_process = chunk_sections;
    } else {
      // Not at end - leave last section for next chunk
      if (chunk_sections.size() > 1) {
        sections_to_process.assign(chunk_sections.begin(),
                                   chunk_sections.end() - 1);
      } else {
        // Only one section - leave it for next chunk
        current_offset = chunk_sections[0].start_offset;
        continue;
      }
    }

    if (sections_to_process.empty()) {
      current_offset += current_chunk_size;
      continue;
    }

    /* CRITICAL TDC PROCESSING LOGIC - DO NOT MODIFY WITHOUT UNDERSTANDING:
     *
     * TDC inheritance is per-chip and sequential within each section:
     *
     * Within each section:
     * 1. Start with inherited TDC from previous section of same chip (if
     * available)
     * 2. Skip hit packets until first TDC packet is encountered (if no
     * inherited TDC)
     * 3. When TDC packet found → use that TDC for subsequent hits in same
     * section
     * 4. TDC only affects hits that come AFTER it in the same section
     * 5. Update m_ChipTdcState[chip] with final TDC for inheritance by future
     * sections
     *
     * Example:
     * Section 0 (chip 0): no inherited TDC → find TDC=1000 → use for hits →
     * save to m_ChipTdcState[0] Section 1 (chip 1): no inherited TDC → find
     * TDC=2000 → use for hits → save to m_ChipTdcState[1] Section 2 (chip 0):
     * inherit TDC=1000 → process hits immediately → update m_ChipTdcState[0] if
     * new TDC found
     *
     * This ensures correct TOF calculation and maintains TDC continuity across
     * sections.
     */

    // Inherit TDC state from previous processing of the same chip AND scan for
    // TDC updates
    for (auto& section : sections_to_process) {
      // First, inherit TDC from previous section of same chip
      if (m_ChipHasTdc[section.chip_id]) {
        section.initial_tdc_timestamp = m_ChipTdcState[section.chip_id];
        section.has_initial_tdc = true;
      } else {
        section.initial_tdc_timestamp = 0;
        section.has_initial_tdc = false;
      }

      // Then scan this section for TDC updates and update global state
      section.start_offset -= current_offset;
      section.end_offset -= current_offset;
      scanSectionForTdc(mapped_file->data(), section, m_ChipTdcState,
                        m_ChipHasTdc);
      section.start_offset += current_offset;
      section.end_offset += current_offset;
    }

    // Process sections (parallel or sequential)
    std::vector<TDCHit> chunk_hits;

    // Pre-allocate chunk_hits based on sections size (Finding 1 optimization)
    size_t chunk_total_packets = 0;
    for (const auto& section : sections_to_process) {
      chunk_total_packets += (section.end_offset - section.start_offset) / 8;
    }
    size_t chunk_estimated_hits =
        static_cast<size_t>(chunk_total_packets * 0.7);
    chunk_hits.reserve(chunk_estimated_hits);
    if (parallel && sections_to_process.size() > 1) {
      // Adjust offsets back to chunk-relative for processing
      for (auto& section : sections_to_process) {
        section.start_offset -= current_offset;
        section.end_offset -= current_offset;
      }
      chunk_hits = processSectionsParallel(mapped_file->data(),
                                           sections_to_process, num_threads);
    } else {
      // Sequential processing
      for (auto& section : sections_to_process) {
        section.start_offset -= current_offset;
        section.end_offset -= current_offset;
        auto section_hits = processSection(mapped_file->data(), section);
        chunk_hits.insert(chunk_hits.end(), section_hits.begin(),
                          section_hits.end());
        total_packets += (section.end_offset - section.start_offset) / 8;
      }
    }

    // Accumulate hits from this chunk
    all_hits.insert(all_hits.end(), chunk_hits.begin(), chunk_hits.end());

    // Move to next chunk (start from beginning of last unprocessed section)
    if (current_offset + current_chunk_size >= file_size) {
      // Reached end of file
      break;
    } else {
      // Start next chunk from the section we left behind
      current_offset = chunk_sections.back().start_offset;
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);

  updateMetrics(duration, all_hits.size(), total_packets);

  return all_hits;
}

std::vector<TDCSection> TDCProcessor::discoverSections(const uint8_t* data,
                                                       size_t size) {
  if (size == 0) {
    return {};
  }

  std::vector<TDCSection> sections;

  size_t current_section_start = 0;
  uint8_t current_chip_id = 0;
  bool in_section = false;

  // Scan entire file for TPX3 headers
  for (size_t offset = 0; offset + 7 < size; offset += 8) {
    // Direct pointer cast (TPX3 files are 8-byte aligned)
    const uint64_t* packet_ptr =
        reinterpret_cast<const uint64_t*>(data + offset);
    uint64_t packet = *packet_ptr;

    if (isTPX3Header(packet)) {
      // Complete previous section if any
      if (in_section && offset > current_section_start) {
        TDCSection section;
        section.start_offset = current_section_start;
        section.end_offset = offset;
        section.chip_id = current_chip_id;
        sections.push_back(section);
      }

      // Start new section
      current_section_start = offset;
      current_chip_id = extractChipId(packet);
      in_section = true;
    }
  }

  // Complete final section
  if (in_section && size > current_section_start) {
    TDCSection section;
    section.start_offset = current_section_start;
    section.end_offset = size;
    section.chip_id = current_chip_id;
    sections.push_back(section);
  }

  return sections;
}

std::vector<TDCHit> TDCProcessor::processSectionsParallel(
    const uint8_t* data, const std::vector<TDCSection>& sections,
    size_t num_threads) {
  if (sections.empty()) {
    return {};
  }

  // Set up TBB task arena with specified number of threads
  size_t actual_threads =
      (num_threads == 0) ? tbb::task_arena::automatic : num_threads;
  tbb::task_arena arena(actual_threads);

  // Calculate estimated hits for pre-allocation (Finding 5 optimization)
  size_t estimated_total_packets = 0;
  for (const auto& section : sections) {
    estimated_total_packets += (section.end_offset - section.start_offset) / 8;
  }
  size_t estimated_hits = static_cast<size_t>(estimated_total_packets * 0.7);

  // Use TBB combinable for thread-local hit storage to avoid synchronization
  tbb::combinable<std::vector<TDCHit>> thread_local_hits;

  // Optimal batched work-stealing: Best performance from our testing (33.7 M
  // hits/sec)
  std::atomic<size_t> section_index{0};
  const size_t sections_per_batch = std::max(
      1UL,
      sections.size() /
          (actual_threads * 200));  // 200 batches per thread - optimal balance

  arena.execute([&] {
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, actual_threads),
        [&](const tbb::blocked_range<size_t>& /* thread_range */) {
          // Get thread-local hit vector
          auto& local_hits = thread_local_hits.local();

          // Pre-allocate thread-local vector on first access (Finding 5
          // optimization)
          if (local_hits.capacity() == 0) {
            size_t thread_estimated_hits = estimated_hits / actual_threads;
            local_hits.reserve(thread_estimated_hits);
          }

          // Work-stealing loop: each thread grabs batch of sections
          size_t batch_start;
          while ((batch_start = section_index.fetch_add(sections_per_batch)) <
                 sections.size()) {
            size_t batch_end =
                std::min(batch_start + sections_per_batch, sections.size());

            // Process batch of sections
            for (size_t i = batch_start; i < batch_end; ++i) {
              auto section_hits = processSection(data, sections[i]);

              // Append to thread-local vector (no synchronization needed)
              local_hits.insert(local_hits.end(), section_hits.begin(),
                                section_hits.end());
            }
          }
        });
  });

  // Combine all thread-local results into final vector
  std::vector<TDCHit> all_hits;

  // Pre-allocate final vector (using calculation from above)
  all_hits.reserve(estimated_hits);

  // Combine all thread-local vectors
  thread_local_hits.combine_each(
      [&all_hits](const std::vector<TDCHit>& local_hits) {
        all_hits.insert(all_hits.end(), local_hits.begin(), local_hits.end());
      });

  return all_hits;
}

void TDCProcessor::scanSectionForTdc(const uint8_t* data, TDCSection& section,
                                     std::array<uint32_t, 4>& chip_tdc_state,
                                     std::array<bool, 4>& chip_has_tdc) {
  // Scan packets in section for TDC updates
  uint32_t current_tdc = section.initial_tdc_timestamp;
  bool found_tdc = section.has_initial_tdc;

  for (size_t offset = section.start_offset + 8;  // Skip header
       offset + 7 < section.end_offset; offset += 8) {
    // Direct pointer cast for TDC scanning
    const uint64_t* packet_ptr =
        reinterpret_cast<const uint64_t*>(data + offset);
    uint64_t packet_data = *packet_ptr;

    TPX3Packet packet(packet_data);

    if (packet.isTDC()) {
      current_tdc = packet.getTDCTimestamp();
      found_tdc = true;
    }
  }

  section.final_tdc_timestamp = current_tdc;
  chip_tdc_state[section.chip_id] = current_tdc;
  chip_has_tdc[section.chip_id] = found_tdc;
}

std::vector<TDCHit> TDCProcessor::processSection(const uint8_t* data,
                                                 const TDCSection& section) {
  std::vector<TDCHit> hits;

  // Pre-allocate based on section size (assume ~70% of packets are hits)
  size_t section_packets = (section.end_offset - section.start_offset) / 8;
  size_t estimated_hits = static_cast<size_t>(section_packets * 0.7);
  hits.reserve(estimated_hits);

  uint32_t current_tdc = section.initial_tdc_timestamp;
  bool has_tdc = section.has_initial_tdc;
  uint8_t chip_id = section.chip_id;

  // Skip first packet (TPX3 header)
  for (size_t offset = section.start_offset + 8;
       offset + 7 < section.end_offset; offset += 8) {
    // Direct pointer cast for hit processing
    const uint64_t* packet_ptr =
        reinterpret_cast<const uint64_t*>(data + offset);
    uint64_t packet_data = *packet_ptr;

    processPacket(packet_data, current_tdc, has_tdc, chip_id, hits);
  }

  return hits;
}

void TDCProcessor::processPacket(uint64_t packet_data, uint32_t& current_tdc,
                                 bool& has_tdc, uint8_t chip_id,
                                 std::vector<TDCHit>& hits) {
  TPX3Packet packet(packet_data);

  if (packet.isTDC()) {
    // Update TDC timestamp
    current_tdc = packet.getTDCTimestamp();
    has_tdc = true;
  } else if (packet.isHit() && has_tdc) {
    // Process hit only if we have a valid TDC
    // Note: convertPacketToHit handles rollover, TOF calculation, and
    // coordinate mapping
    TDCHit hit = convertPacketToHit(packet, chip_id, current_tdc, m_Config,
                                    m_MissingTdcCorrectionEnabled);
    hits.push_back(hit);
  }
  // Ignore other packet types and hits before first TDC
}

uint32_t TDCProcessor::applyTdcCorrection(uint32_t tof) const {
  // From Python: if TOF*25/1e9 > 1/TDC_frequency: TOF = TOF -
  // (1/TDC_frequency)*1e9/25
  double tof_seconds = tof * 25e-9;  // Convert to seconds
  double tdc_period = 1.0 / m_Config.getTdcFrequency();

  if (tof_seconds > tdc_period) {
    // Need correction: subtract one TDC period
    uint32_t correction = static_cast<uint32_t>(tdc_period * 1e9 / 25 + 0.5);
    return tof - correction;
  }

  return tof;  // No correction needed
}

uint32_t TDCProcessor::handleRollover(uint32_t hit_timestamp,
                                      uint32_t tdc_timestamp) {
  // From Python: if Timestamp25ns + 0x400000 < TDC_Timestamp25ns:
  //                Timestamp25ns = Timestamp25ns | 0x40000000
  if ((hit_timestamp + 0x400000) < tdc_timestamp) {
    return hit_timestamp | 0x40000000;  // Set bit 30
  }

  return hit_timestamp;
}

void TDCProcessor::updateMetrics(std::chrono::microseconds processing_time,
                                 size_t hit_count, size_t packet_count) {
  m_LastProcessingTimeMs = processing_time.count() / 1000.0;
  m_LastHitCount = hit_count;
  m_LastPacketCount = packet_count;

  if (processing_time.count() > 0) {
    m_LastHitsPerSecond = (hit_count * 1e6) / processing_time.count();
  } else {
    m_LastHitsPerSecond = 0.0;
  }
}

size_t TDCProcessor::writeHitsToHDF5(const std::string& h5_path,
                                     const std::vector<TDCHit>& hits,
                                     size_t current_offset) {
  if (hits.empty()) {
    return current_offset;
  }

  try {
    // Open or create HDF5 file
    // NOTE: File locking is not used - do not call this method concurrently
    // from multiple threads/processes with the same output file path
    H5::H5File file;
    bool need_create_datasets = false;

    try {
      // Try to open existing file
      file = H5::H5File(h5_path, H5F_ACC_RDWR);

      // Check if datasets exist by trying to open one
      try {
        H5::DataSet test_ds = file.openDataSet("tof");
        test_ds.close();
        need_create_datasets = false;
      } catch (const H5::Exception&) {
        // Dataset doesn't exist, need to create
        need_create_datasets = true;
      }
    } catch (const H5::Exception&) {
      // File doesn't exist, create it
      file = H5::H5File(h5_path, H5F_ACC_TRUNC);
      need_create_datasets = true;
    }

    // Dataset names for each field
    const std::vector<std::string> field_names = {
        "tof", "x", "y", "timestamp", "tot", "chip_id", "cluster_id"};

    // Create datasets only if they don't exist
    if (need_create_datasets) {
      // Create datasets with chunking for efficient appending
      hsize_t initial_dims[1] = {0};
      hsize_t max_dims[1] = {H5S_UNLIMITED};
      hsize_t chunk_dims[1] = {65536};  // 64K hits per chunk

      H5::DataSpace dataspace(1, initial_dims, max_dims);

      // Create dataset creation property list for chunking
      H5::DSetCreatPropList prop;
      prop.setChunk(1, chunk_dims);
      prop.setDeflate(6);  // gzip compression level 6

      // Create datasets for each field
      file.createDataSet("tof", H5::PredType::NATIVE_UINT32, dataspace, prop);
      file.createDataSet("x", H5::PredType::NATIVE_UINT16, dataspace, prop);
      file.createDataSet("y", H5::PredType::NATIVE_UINT16, dataspace, prop);
      file.createDataSet("timestamp", H5::PredType::NATIVE_UINT32, dataspace,
                         prop);
      file.createDataSet("tot", H5::PredType::NATIVE_UINT16, dataspace, prop);
      file.createDataSet("chip_id", H5::PredType::NATIVE_UINT8, dataspace,
                         prop);
      file.createDataSet("cluster_id", H5::PredType::NATIVE_INT32, dataspace,
                         prop);

      // Add metadata attributes
      H5::Group root = file.openGroup("/");
      H5::DataSpace attr_space(H5S_SCALAR);

      // Processor version
      H5::StrType str_type(H5::PredType::C_S1, 64);
      H5::Attribute attr =
          root.createAttribute("processor", str_type, attr_space);
      std::string processor_name = "TDCSophiread";
      attr.write(str_type, processor_name);

      // TDC frequency
      H5::Attribute freq_attr = root.createAttribute(
          "tdc_frequency_hz", H5::PredType::NATIVE_DOUBLE, attr_space);
      double tdc_freq = m_Config.getTdcFrequency();
      freq_attr.write(H5::PredType::NATIVE_DOUBLE, &tdc_freq);

      // Missing TDC correction enabled
      H5::Attribute corr_attr =
          root.createAttribute("missing_tdc_correction_enabled",
                               H5::PredType::NATIVE_HBOOL, attr_space);
      bool corr_enabled = m_MissingTdcCorrectionEnabled;
      corr_attr.write(H5::PredType::NATIVE_HBOOL, &corr_enabled);
    }

    // Prepare data buffers for each field
    std::vector<uint32_t> tof_data(hits.size());
    std::vector<uint16_t> x_data(hits.size());
    std::vector<uint16_t> y_data(hits.size());
    std::vector<uint32_t> timestamp_data(hits.size());
    std::vector<uint16_t> tot_data(hits.size());
    std::vector<uint8_t> chip_id_data(hits.size());
    std::vector<int32_t> cluster_id_data(hits.size());

    // Copy data from TDCHit struct to separate arrays
    for (size_t i = 0; i < hits.size(); ++i) {
      tof_data[i] = hits[i].tof;
      x_data[i] = hits[i].x;
      y_data[i] = hits[i].y;
      timestamp_data[i] = hits[i].timestamp;
      tot_data[i] = hits[i].tot;
      chip_id_data[i] = hits[i].chip_id;
      cluster_id_data[i] = hits[i].cluster_id;
    }

    // Extend and write each dataset
    size_t new_size = current_offset + hits.size();

    // Helper lambda to extend and write dataset
    auto extend_and_write = [&](const std::string& name, auto& data,
                                const H5::DataType& dtype) {
      H5::DataSet dataset = file.openDataSet(name);

      // Extend dataset
      hsize_t new_dims[1] = {new_size};
      dataset.extend(new_dims);

      // Select hyperslab for new data
      H5::DataSpace filespace = dataset.getSpace();
      hsize_t offset[1] = {current_offset};
      hsize_t count[1] = {hits.size()};
      filespace.selectHyperslab(H5S_SELECT_SET, count, offset);

      // Create memory dataspace
      hsize_t mem_dims[1] = {hits.size()};
      H5::DataSpace memspace(1, mem_dims);

      // Write data
      dataset.write(data.data(), dtype, memspace, filespace);
    };

    // Write all fields
    extend_and_write("tof", tof_data, H5::PredType::NATIVE_UINT32);
    extend_and_write("x", x_data, H5::PredType::NATIVE_UINT16);
    extend_and_write("y", y_data, H5::PredType::NATIVE_UINT16);
    extend_and_write("timestamp", timestamp_data, H5::PredType::NATIVE_UINT32);
    extend_and_write("tot", tot_data, H5::PredType::NATIVE_UINT16);
    extend_and_write("chip_id", chip_id_data, H5::PredType::NATIVE_UINT8);
    extend_and_write("cluster_id", cluster_id_data, H5::PredType::NATIVE_INT32);

    file.close();
    return new_size;

  } catch (const H5::Exception& e) {
    throw std::runtime_error("HDF5 error in writeHitsToHDF5: " +
                             std::string(e.getDetailMsg()));
  }
}

TDCProcessor::StreamingResult TDCProcessor::processFileToHDF5(
    const std::string& file_path, const std::string& output_h5_path,
    size_t chunk_size_mb, bool parallel, size_t num_threads) {
  auto start_time = std::chrono::high_resolution_clock::now();

  StreamingResult result;

  try {
    // Get total file size for chunking
    std::error_code ec;
    auto file_size = std::filesystem::file_size(file_path, ec);
    if (ec) {
      result.success = false;
      result.error_message = "Cannot determine file size: " + file_path;
      return result;
    }

    if (file_size == 0) {
      result.success = true;
      return result;
    }

    // Convert chunk size to bytes
    size_t chunk_size_bytes = chunk_size_mb * 1024 * 1024;

    // Reset TDC state for this file
    m_ChipTdcState = {0, 0, 0, 0};
    m_ChipHasTdc = {false, false, false, false};

    size_t current_offset = 0;
    size_t hdf5_offset = 0;  // Track position in HDF5 file

    // Delete output file if it exists to ensure clean start
    // WARNING: This will permanently delete any existing file at this path
    if (std::filesystem::exists(output_h5_path)) {
      std::cerr << "Warning: Deleting existing output file at '" << output_h5_path << "'." << std::endl;
      std::filesystem::remove(output_h5_path);
    }

    // Process file in chunks
    while (current_offset < file_size) {
      // Determine chunk size (don't exceed file size)
      size_t remaining = file_size - current_offset;
      size_t current_chunk_size = std::min(chunk_size_bytes, remaining);

      // Map current chunk
      auto mapped_file =
          MappedFile::open(file_path, current_offset, current_chunk_size);

      // Find sections within this chunk
      auto chunk_sections =
          discoverSections(mapped_file->data(), mapped_file->size());

      if (chunk_sections.empty()) {
        // No sections found, advance to next chunk
        current_offset += current_chunk_size;
        continue;
      }

      // Adjust section offsets to be relative to file start
      for (auto& section : chunk_sections) {
        section.start_offset += current_offset;
        section.end_offset += current_offset;
      }

      // Apply "always leave last section" strategy (unless we're at end of
      // file)
      std::vector<TDCSection> sections_to_process;
      bool at_end_of_file = (current_offset + current_chunk_size >= file_size);

      if (at_end_of_file) {
        // At end of file - process all sections
        sections_to_process = chunk_sections;
      } else {
        // Not at end - leave last section for next chunk
        if (chunk_sections.size() > 1) {
          sections_to_process.assign(chunk_sections.begin(),
                                     chunk_sections.end() - 1);
        } else {
          // Only one section - leave it for next chunk
          current_offset = chunk_sections[0].start_offset;
          continue;
        }
      }

      if (sections_to_process.empty()) {
        current_offset += current_chunk_size;
        continue;
      }

      // Inherit TDC state and scan for updates (same as processFile)
      for (auto& section : sections_to_process) {
        if (m_ChipHasTdc[section.chip_id]) {
          section.initial_tdc_timestamp = m_ChipTdcState[section.chip_id];
          section.has_initial_tdc = true;
        } else {
          section.initial_tdc_timestamp = 0;
          section.has_initial_tdc = false;
        }

        section.start_offset -= current_offset;
        section.end_offset -= current_offset;
        scanSectionForTdc(mapped_file->data(), section, m_ChipTdcState,
                          m_ChipHasTdc);
        section.start_offset += current_offset;
        section.end_offset += current_offset;
      }

      // Store the next chunk start offset before we modify section offsets
      size_t next_chunk_start = chunk_sections.back().start_offset;

      // Process sections (parallel or sequential)
      std::vector<TDCHit> chunk_hits;

      if (parallel && sections_to_process.size() > 1) {
        // Adjust offsets back to chunk-relative for processing
        for (auto& section : sections_to_process) {
          section.start_offset -= current_offset;
          section.end_offset -= current_offset;
        }
        chunk_hits = processSectionsParallel(mapped_file->data(),
                                             sections_to_process, num_threads);
      } else {
        // Sequential processing
        for (auto& section : sections_to_process) {
          section.start_offset -= current_offset;
          section.end_offset -= current_offset;
          auto section_hits = processSection(mapped_file->data(), section);
          chunk_hits.insert(chunk_hits.end(), section_hits.begin(),
                            section_hits.end());
          result.total_packets +=
              (section.end_offset - section.start_offset) / 8;
        }
      }

      // Write chunk hits to HDF5 immediately (bounded memory!)
      hdf5_offset = writeHitsToHDF5(output_h5_path, chunk_hits, hdf5_offset);
      result.total_hits += chunk_hits.size();

      // chunk_hits goes out of scope here, freeing memory

      // Move to next chunk
      if (current_offset + current_chunk_size >= file_size) {
        break;
      } else {
        current_offset = next_chunk_start;
      }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);

    result.processing_time_ms = duration.count() / 1000.0;
    if (duration.count() > 0) {
      result.hits_per_second = (result.total_hits * 1e6) / duration.count();
    }

    result.success = true;
    updateMetrics(duration, result.total_hits, result.total_packets);

  } catch (const std::exception& e) {
    result.success = false;
    result.error_message = "Processing failed: " + std::string(e.what());
  }

  return result;
}

}  // namespace tdcsophiread