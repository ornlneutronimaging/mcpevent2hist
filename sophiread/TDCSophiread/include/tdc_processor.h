// TDCSophiread Section-Aware Processor Header
// Implements two-phase processing: TDC propagation + parallel section
// processing

#pragma once

#include <array>
#include <chrono>
#include <string>
#include <vector>

#include "tdc_detector_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

/**
 * @brief Represents a contiguous section of TPX3 data between headers
 *
 * TPX3 data is organized in sections, each starting with a TPX3 header
 * that identifies the chip. All packets following a header belong to
 * that chip until the next header is encountered.
 */
struct TDCSection {
  size_t start_offset;             // Byte offset where section starts
  size_t end_offset;               // Byte offset where section ends (exclusive)
  uint8_t chip_id;                 // Chip ID from TPX3 header (0-3)
  uint32_t initial_tdc_timestamp;  // TDC timestamp at section start
  uint32_t final_tdc_timestamp;    // TDC timestamp at section end
  bool has_initial_tdc = false;    // Whether initial TDC is valid
};

/**
 * @brief Section-aware TDC processor for TPX3 files
 *
 * This processor implements the two-phase strategy from TPX3_DATA_STRUCTURE.md:
 *
 * Phase 1 (Sequential): Discover sections and propagate TDC timestamps
 * - Scan for TPX3 headers to identify section boundaries
 * - Track per-chip TDC state across sections
 * - Each section inherits TDC from previous section of same chip
 *
 * Phase 2 (Parallel-ready): Process sections independently
 * - Each section knows its initial TDC state
 * - No inter-section communication needed
 * - Ready for TBB parallelization
 *
 * The processor respects section boundaries during chunk processing,
 * ensuring no data loss from arbitrary file splitting.
 */
class TDCProcessor {
 public:
  /**
   * @brief Construct processor with detector configuration
   * @param config DetectorConfig for coordinate mapping and timing parameters
   */
  explicit TDCProcessor(const DetectorConfig& config);

  /**
   * @brief Process TPX3 file with chunk-based memory mapping
   * @param file_path Path to TPX3 file to process
   * @param chunk_size_mb Chunk size in megabytes (default: 512MB)
   * @param parallel Enable TBB parallelization (default: false)
   * @param num_threads Number of TBB threads (0 = auto-detect, default: 0)
   * @return Vector of TDCHit objects representing all detected hits
   * @throws std::runtime_error if file cannot be read or is invalid
   */
  std::vector<TDCHit> processFile(const std::string& file_path,
                                  size_t chunk_size_mb = 512,
                                  bool parallel = false,
                                  size_t num_threads = 0);

  /**
   * @brief Discover all sections in TPX3 data
   * @param data Pointer to TPX3 data in memory
   * @param size Size of data in bytes
   * @return Vector of TDCSection structures describing all sections
   *
   * Scans the data for TPX3 headers (magic: 0x33585054) and
   * creates section boundaries. This is Phase 1 of processing.
   */
  std::vector<TDCSection> discoverSections(const uint8_t* data, size_t size);

  // ==================== PERFORMANCE METRICS ====================

  /**
   * @brief Get processing time for last operation in milliseconds
   */
  double getLastProcessingTimeMs() const { return m_LastProcessingTimeMs; }

  /**
   * @brief Get number of hits processed in last operation
   */
  size_t getLastHitCount() const { return m_LastHitCount; }

  /**
   * @brief Get processing rate for last operation
   */
  double getLastHitsPerSecond() const { return m_LastHitsPerSecond; }

  /**
   * @brief Get total number of packets processed in last operation
   */
  size_t getLastPacketCount() const { return m_LastPacketCount; }

  // ==================== CONFIGURATION ====================

  /**
   * @brief Enable or disable missing TDC correction
   * @param enable True to enable correction (default from config)
   */
  void setMissingTdcCorrectionEnabled(bool enable) {
    m_MissingTdcCorrectionEnabled = enable;
  }

  // ==================== PHASE 2: HIT PROCESSING ====================

  /**
   * @brief Process a single section to extract hits
   * @param data Memory-mapped file data
   * @param section Section to process
   * @return Vector of hits from this section
   *
   * This method can be parallelized as each section has complete TDC state
   */
  std::vector<TDCHit> processSection(const uint8_t* data,
                                     const TDCSection& section);

  /**
   * @brief Process multiple sections in parallel using TBB
   * @param data Memory-mapped file data
   * @param sections Vector of sections to process
   * @param num_threads Number of TBB threads (0 = auto-detect)
   * @return Vector of all hits from all sections
   *
   * Uses TBB parallel_for with thread-local hit vectors to avoid
   * synchronization overhead during hit collection.
   */
  std::vector<TDCHit> processSectionsParallel(
      const uint8_t* data, const std::vector<TDCSection>& sections,
      size_t num_threads = 0);

 private:
  // ==================== PHASE 1: TDC PROPAGATION ====================

  /**
   * @brief Scan section for TDC packets and update timestamps
   * @param data Memory-mapped file data
   * @param section Section to scan
   * @param chip_tdc_state Per-chip TDC state to update
   */
  void scanSectionForTdc(const uint8_t* data, TDCSection& section,
                         std::array<uint32_t, 4>& chip_tdc_state,
                         std::array<bool, 4>& chip_has_tdc);

  /**
   * @brief Process single packet within a section
   * @param packet_data 64-bit packet data
   * @param current_tdc Current TDC timestamp for this section
   * @param has_tdc Whether TDC has been seen
   * @param chip_id Current chip ID
   * @param hits Output vector to append hits to
   */
  void processPacket(uint64_t packet_data, uint32_t& current_tdc, bool& has_tdc,
                     uint8_t chip_id, std::vector<TDCHit>& hits);

  // ==================== HELPER METHODS ====================

  /**
   * @brief Check if packet is a TPX3 header
   * @param packet 64-bit packet data
   * @return True if packet has TPX3 magic number
   */
  static bool isTPX3Header(uint64_t packet) {
    return (packet & 0xFFFFFFFF) == 0x33585054;
  }

  /**
   * @brief Extract chip ID from TPX3 header packet
   * @param packet TPX3 header packet
   * @return Chip ID (0-3)
   */
  static uint8_t extractChipId(uint64_t packet) {
    return (packet >> 32) & 0xFF;
  }

  /**
   * @brief Apply missing TDC correction algorithm
   * @param tof Time-of-flight in 25ns units
   * @return Corrected TOF
   *
   * From Python reference:
   * if TOF*25/1e9 > 1/TDC_frequency:
   *     TOF = TOF - (1/TDC_frequency)*1e9/25
   */
  uint32_t applyTdcCorrection(uint32_t tof) const;

  /**
   * @brief Handle timestamp rollover detection
   * @param hit_timestamp Hit timestamp (30-bit)
   * @param tdc_timestamp Current TDC timestamp
   * @return Extended timestamp with rollover correction
   *
   * From Python reference:
   * if Timestamp25ns + 0x400000 < TDC_Timestamp25ns:
   *     Timestamp25ns = Timestamp25ns | 0x40000000
   */
  static uint32_t handleRollover(uint32_t hit_timestamp,
                                 uint32_t tdc_timestamp);

  /**
   * @brief Update performance metrics after processing
   */
  void updateMetrics(std::chrono::microseconds processing_time,
                     size_t hit_count, size_t packet_count);

  // ==================== MEMBER VARIABLES ====================

  // Configuration
  const DetectorConfig& m_Config;
  bool m_MissingTdcCorrectionEnabled;

  // Per-chip TDC state for chunk processing continuity
  std::array<uint32_t, 4> m_ChipTdcState = {0, 0, 0, 0};
  std::array<bool, 4> m_ChipHasTdc = {false, false, false, false};

  // Performance metrics
  double m_LastProcessingTimeMs = 0.0;
  size_t m_LastHitCount = 0;
  double m_LastHitsPerSecond = 0.0;
  size_t m_LastPacketCount = 0;
};

}  // namespace tdcsophiread