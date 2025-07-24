// TDCSophiread Streaming Configuration Header
// TDD Step 2: Minimal interface to make tests compile

#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace tdcsophiread {

/**
 * @brief Processing mode for streaming
 */
enum class ProcessingMode {
  IMAGING,     // TOF histogram accumulation, constant memory
  RADIOGRAPHY  // Clustering with bounded memory
};

/**
 * @brief TOF binning configuration
 */
struct TOFBinningConfig {
  size_t num_bins = 1500;
  double range_seconds = 1.0 / 60.0;  // 60Hz default

  double getBinWidth() const;
  size_t timeToBin(double time_seconds) const;
  double binToTime(size_t bin_index) const;
};

/**
 * @brief Streaming configuration for memory-efficient processing
 *
 * TDD Step 2: Minimal interface - implementation will make tests pass
 */
class StreamingConfig {
 public:
  // Configuration fields
  ProcessingMode mode = ProcessingMode::RADIOGRAPHY;

  // Memory management
  size_t max_hits_in_memory = 50000000;      // ~1.2GB
  size_t max_neutrons_in_memory = 10000000;  // ~240MB
  size_t chunk_size_mb = 512;

  // Mode-specific settings
  bool enable_tof_histograms = false;
  bool save_hits_to_disk = false;
  bool save_neutrons_to_disk = true;
  double temporal_window_ms = 0.04;  // 40μs
  size_t tiff_save_interval_sec = 30;

  // Progress reporting
  bool enable_progress_callbacks = true;

  // HDF5 settings
  int compression_level = 6;
  size_t write_buffer_size_mb = 100;

  // TOF binning
  TOFBinningConfig tof_binning;

  // Factory methods
  static StreamingConfig imagingDefaults();
  static StreamingConfig radiographyDefaults();
  static StreamingConfig forLimitedMemory(size_t memory_limit_mb);
  static StreamingConfig forHighThroughput();

  // Memory estimation
  size_t estimateHitMemoryUsage() const;
  size_t estimateNeutronMemoryUsage() const;
  size_t estimateTotalMemoryUsage() const;

  // Mode queries
  bool isImagingMode() const;
  bool isRadiographyMode() const;
  std::string getModeString() const;

  // Validation
  bool isValid() const;

  // JSON serialization
  nlohmann::json toJson() const;
  static StreamingConfig fromJson(const nlohmann::json& j);

  // File I/O
  void saveToFile(const std::string& filename) const;
  static StreamingConfig fromFile(const std::string& filename);
};

}  // namespace tdcsophiread