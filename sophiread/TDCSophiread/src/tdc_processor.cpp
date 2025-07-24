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

}  // namespace tdcsophiread