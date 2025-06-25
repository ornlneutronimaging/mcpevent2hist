// TDCSophiread Section-Aware Processor Implementation
// Implements two-phase processing: TDC propagation + section processing

#include "tdc_processor.h"

#include <tbb/blocked_range.h>
#include <tbb/combinable.h>
#include <tbb/parallel_for.h>
#include <tbb/task_arena.h>

#include <algorithm>
#include <cstring>
#include <execution>
#include <stdexcept>

#include "tdc_io.h"
#include "tdc_packet.h"

namespace tdcsophiread {

TDCProcessor::TDCProcessor(const DetectorConfig& config)
    : m_Config(config),
      m_MissingTdcCorrectionEnabled(config.isMissingTdcCorrectionEnabled()) {}

std::vector<TDCHit> TDCProcessor::processFile(const std::string& file_path) {
  auto start_time = std::chrono::high_resolution_clock::now();

  // Open file once
  auto mapped_file = MappedFile::open(file_path);

  // Phase 1: Discover sections
  auto sections = discoverSections(file_path);

  if (sections.empty()) {
    updateMetrics(std::chrono::microseconds(0), 0, 0);
    return {};
  }

  // Phase 1: Propagate TDC state across sections
  // Per-chip TDC state tracking
  std::array<uint32_t, 4> chip_tdc_state = {0, 0, 0, 0};
  std::array<bool, 4> chip_has_tdc = {false, false, false, false};

  for (auto& section : sections) {
    // Inherit TDC from previous section of same chip
    if (chip_has_tdc[section.chip_id]) {
      section.initial_tdc_timestamp = chip_tdc_state[section.chip_id];
      section.has_initial_tdc = true;
    } else {
      section.initial_tdc_timestamp = 0;
      section.has_initial_tdc = false;
    }

    // Scan section for TDC updates
    scanSectionForTdc(mapped_file->data(), section, chip_tdc_state,
                      chip_has_tdc);
  }

  // Phase 2: Process sections (single-threaded for now)
  std::vector<TDCHit> all_hits;
  size_t total_packets = 0;

  for (const auto& section : sections) {
    auto section_hits = processSection(mapped_file->data(), section);
    all_hits.insert(all_hits.end(), section_hits.begin(), section_hits.end());
    total_packets += (section.end_offset - section.start_offset) / 8;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);

  updateMetrics(duration, all_hits.size(), total_packets);

  return all_hits;
}

std::vector<TDCHit> TDCProcessor::processFileParallel(
    const std::string& file_path, size_t num_threads) {
  auto start_time = std::chrono::high_resolution_clock::now();

  // Open file once
  auto mapped_file = MappedFile::open(file_path);

  // Phase 1: Discover sections (must be sequential)
  auto sections = discoverSections(file_path);

  if (sections.empty()) {
    updateMetrics(std::chrono::microseconds(0), 0, 0);
    return {};
  }

  // Phase 1: Propagate TDC state across sections (must be sequential)
  std::array<uint32_t, 4> chip_tdc_state = {0, 0, 0, 0};
  std::array<bool, 4> chip_has_tdc = {false, false, false, false};

  for (auto& section : sections) {
    // Inherit TDC from previous section of same chip
    if (chip_has_tdc[section.chip_id]) {
      section.initial_tdc_timestamp = chip_tdc_state[section.chip_id];
      section.has_initial_tdc = true;
    } else {
      section.initial_tdc_timestamp = 0;
      section.has_initial_tdc = false;
    }

    // Scan section for TDC updates
    scanSectionForTdc(mapped_file->data(), section, chip_tdc_state,
                      chip_has_tdc);
  }

  // Phase 2: Process sections in parallel
  auto all_hits =
      processSectionsParallel(mapped_file->data(), sections, num_threads);

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);

  // Calculate total packets for metrics
  size_t total_packets = 0;
  for (const auto& section : sections) {
    total_packets += (section.end_offset - section.start_offset) / 8;
  }

  updateMetrics(duration, all_hits.size(), total_packets);

  return all_hits;
}

std::vector<TDCHit> TDCProcessor::processChunk(const std::string& file_path,
                                               size_t start_offset,
                                               size_t requested_size,
                                               size_t& actual_processed) {
  auto start_time = std::chrono::high_resolution_clock::now();

  // Open file
  auto mapped_file = MappedFile::open(file_path);

  if (start_offset >= mapped_file->size()) {
    actual_processed = 0;
    return {};
  }

  // Simple smart chunking: find complete sections within requested range
  const uint8_t* data = mapped_file->data();
  std::vector<TDCSection> chunk_sections;

  size_t current_offset = start_offset;
  size_t chunk_limit =
      std::min(start_offset + requested_size, mapped_file->size());

  // Scan for TPX3 headers within the requested chunk
  while (current_offset + 7 < chunk_limit) {
    uint64_t packet;
    std::memcpy(&packet, data + current_offset, sizeof(packet));

    if (isTPX3Header(packet)) {
      // Found a section start
      uint8_t chip_id = extractChipId(packet);
      size_t section_start = current_offset;

      // Find the end of this section (next header or end of data)
      size_t section_end = chunk_limit;
      for (size_t scan = current_offset + 8; scan + 7 < mapped_file->size();
           scan += 8) {
        uint64_t scan_packet;
        std::memcpy(&scan_packet, data + scan, sizeof(scan_packet));
        if (isTPX3Header(scan_packet)) {
          section_end = scan;
          break;
        }
      }

      // Only include this section if it ends within our chunk limit
      if (section_end <= chunk_limit) {
        TDCSection section;
        section.start_offset = section_start;
        section.end_offset = section_end;
        section.chip_id = chip_id;
        chunk_sections.push_back(section);
        current_offset = section_end;
      } else {
        // This section extends beyond our chunk - stop here
        break;
      }
    } else {
      current_offset += 8;
    }
  }

  if (chunk_sections.empty()) {
    actual_processed = 0;
    return {};
  }

  actual_processed = chunk_sections.back().end_offset - start_offset;

  // Inherit TDC state from previous processing
  for (auto& section : chunk_sections) {
    if (m_ChipHasTdc[section.chip_id]) {
      section.initial_tdc_timestamp = m_ChipTdcState[section.chip_id];
      section.has_initial_tdc = true;
    }
  }

  // Propagate TDC within this chunk
  // Use global chip state and scan sections
  for (auto& section : chunk_sections) {
    scanSectionForTdc(data, section, m_ChipTdcState, m_ChipHasTdc);
  }

  // Update global TDC state for next chunk
  for (const auto& section : chunk_sections) {
    if (section.final_tdc_timestamp != section.initial_tdc_timestamp ||
        section.has_initial_tdc) {
      m_ChipTdcState[section.chip_id] = section.final_tdc_timestamp;
      m_ChipHasTdc[section.chip_id] = true;
    }
  }

  // Process sections
  std::vector<TDCHit> all_hits;
  size_t total_packets = 0;

  for (const auto& section : chunk_sections) {
    auto section_hits = processSection(data, section);
    all_hits.insert(all_hits.end(), section_hits.begin(), section_hits.end());
    total_packets += (section.end_offset - section.start_offset) / 8;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);

  updateMetrics(duration, all_hits.size(), total_packets);

  return all_hits;
}

std::vector<TDCSection> TDCProcessor::discoverSections(
    const std::string& file_path) {
  auto mapped_file = MappedFile::open(file_path);

  if (mapped_file->size() == 0) {
    return {};
  }

  std::vector<TDCSection> sections;
  const uint8_t* data = mapped_file->data();
  size_t file_size = mapped_file->size();

  size_t current_section_start = 0;
  uint8_t current_chip_id = 0;
  bool in_section = false;

  // Scan entire file for TPX3 headers
  for (size_t offset = 0; offset + 7 < file_size; offset += 8) {
    uint64_t packet;
    std::memcpy(&packet, data + offset, sizeof(packet));

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
  if (in_section && file_size > current_section_start) {
    TDCSection section;
    section.start_offset = current_section_start;
    section.end_offset = file_size;
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

  // Use TBB combinable for thread-local hit storage to avoid synchronization
  tbb::combinable<std::vector<TDCHit>> thread_local_hits;

  // Process sections in parallel within the task arena
  arena.execute([&] {
    tbb::parallel_for(tbb::blocked_range<size_t>(0, sections.size()),
                      [&](const tbb::blocked_range<size_t>& range) {
                        // Get thread-local hit vector
                        auto& local_hits = thread_local_hits.local();

                        // Process sections assigned to this thread
                        for (size_t i = range.begin(); i != range.end(); ++i) {
                          auto section_hits = processSection(data, sections[i]);

                          // Append to thread-local vector (no synchronization
                          // needed)
                          local_hits.insert(local_hits.end(),
                                            section_hits.begin(),
                                            section_hits.end());
                        }
                      });
  });

  // Combine all thread-local results into final vector
  std::vector<TDCHit> all_hits;

  // Pre-allocate space to avoid multiple reallocations
  size_t total_estimated_hits = 0;
  thread_local_hits.combine_each(
      [&total_estimated_hits](const std::vector<TDCHit>& local_hits) {
        total_estimated_hits += local_hits.size();
      });
  all_hits.reserve(total_estimated_hits);

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
    uint64_t packet_data;
    std::memcpy(&packet_data, data + offset, sizeof(packet_data));

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

  uint32_t current_tdc = section.initial_tdc_timestamp;
  bool has_tdc = section.has_initial_tdc;
  uint8_t chip_id = section.chip_id;

  // Skip first packet (TPX3 header)
  for (size_t offset = section.start_offset + 8;
       offset + 7 < section.end_offset; offset += 8) {
    uint64_t packet_data;
    std::memcpy(&packet_data, data + offset, sizeof(packet_data));

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