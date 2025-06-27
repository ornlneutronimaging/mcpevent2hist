// TDCSophiread Streaming Configuration Implementation
// TDD Step 3: Real implementation to make tests pass

#include "tdc_streaming_config.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace tdcsophiread {

// TOFBinningConfig methods
double TOFBinningConfig::getBinWidth() const {
  if (num_bins == 0) return 0.0;
  return range_seconds / num_bins;
}

size_t TOFBinningConfig::timeToBin(double time_seconds) const {
  if (time_seconds < 0.0 || num_bins == 0) return 0;
  if (time_seconds >= range_seconds) return num_bins - 1;

  size_t bin = static_cast<size_t>(time_seconds / getBinWidth());
  return std::min(bin, num_bins - 1);
}

double TOFBinningConfig::binToTime(size_t bin_index) const {
  if (bin_index >= num_bins) bin_index = num_bins - 1;
  return bin_index * getBinWidth();
}

// StreamingConfig factory methods
StreamingConfig StreamingConfig::imagingDefaults() {
  StreamingConfig config;
  config.mode = ProcessingMode::IMAGING;
  config.max_hits_in_memory = 25000000;  // 25M hits (~600MB)
  config.max_neutrons_in_memory = 0;     // No neutrons in imaging mode
  config.chunk_size_mb = 512;
  config.enable_tof_histograms = true;
  config.save_hits_to_disk = true;       // Per design: default ON for imaging
  config.save_neutrons_to_disk = false;  // No clustering
  config.temporal_window_ms = 0.0;       // Not used in imaging
  config.tiff_save_interval_sec = 30;
  config.enable_progress_callbacks = true;
  config.compression_level = 6;
  config.write_buffer_size_mb = 100;
  config.tof_binning = {1500, 1.0 / 60.0};  // Per design plan
  return config;
}

StreamingConfig StreamingConfig::radiographyDefaults() {
  StreamingConfig config;
  config.mode = ProcessingMode::RADIOGRAPHY;
  config.max_hits_in_memory = 50000000;      // 50M hits (~1.2GB)
  config.max_neutrons_in_memory = 10000000;  // 10M neutrons (~240MB)
  config.chunk_size_mb = 512;
  config.enable_tof_histograms = false;  // No TOF accumulation
  config.save_hits_to_disk = false;      // Default off per design
  config.save_neutrons_to_disk = true;   // Primary output
  config.temporal_window_ms = 0.04;      // 40μs
  config.tiff_save_interval_sec = 0;     // Not used
  config.enable_progress_callbacks = true;
  config.compression_level = 6;
  config.write_buffer_size_mb = 100;
  config.tof_binning = {1500, 1.0 / 60.0};  // Same defaults
  return config;
}

StreamingConfig StreamingConfig::forLimitedMemory(size_t memory_limit_mb) {
  StreamingConfig config = radiographyDefaults();

  // Assume 70% for hits, 30% for neutrons
  size_t available_bytes = memory_limit_mb * 1024 * 1024;
  size_t hit_bytes = available_bytes * 0.7;
  size_t neutron_bytes = available_bytes * 0.3;

  // Estimate hits (assuming ~24 bytes per hit)
  config.max_hits_in_memory = std::max(1000000UL, hit_bytes / 24);

  // Estimate neutrons (assuming ~24 bytes per neutron)
  config.max_neutrons_in_memory = std::max(100000UL, neutron_bytes / 24);

  // Smaller chunks for limited memory
  config.chunk_size_mb = std::max(128UL, memory_limit_mb / 8);

  return config;
}

StreamingConfig StreamingConfig::forHighThroughput() {
  StreamingConfig config = radiographyDefaults();

  // Optimize for speed - larger buffers
  config.max_hits_in_memory = 100000000;     // 100M hits (~2.4GB)
  config.max_neutrons_in_memory = 20000000;  // 20M neutrons (~480MB)
  config.chunk_size_mb = 1024;               // Large chunks
  config.write_buffer_size_mb = 200;         // Larger write buffers

  return config;
}

// Memory estimation
size_t StreamingConfig::estimateHitMemoryUsage() const {
  // Assume TDCHit is ~24 bytes (conservative estimate)
  return max_hits_in_memory * 24;
}

size_t StreamingConfig::estimateNeutronMemoryUsage() const {
  // Assume TDCNeutron is ~24 bytes (conservative estimate)
  return max_neutrons_in_memory * 24;
}

size_t StreamingConfig::estimateTotalMemoryUsage() const {
  return estimateHitMemoryUsage() + estimateNeutronMemoryUsage();
}

// Mode queries
bool StreamingConfig::isImagingMode() const {
  return mode == ProcessingMode::IMAGING;
}

bool StreamingConfig::isRadiographyMode() const {
  return mode == ProcessingMode::RADIOGRAPHY;
}

std::string StreamingConfig::getModeString() const {
  switch (mode) {
    case ProcessingMode::IMAGING:
      return "IMAGING";
    case ProcessingMode::RADIOGRAPHY:
      return "RADIOGRAPHY";
    default:
      return "UNKNOWN";
  }
}

// Validation
bool StreamingConfig::isValid() const {
  // Check basic constraints
  if (max_hits_in_memory == 0) return false;
  if (chunk_size_mb == 0) return false;
  if (compression_level < 0 || compression_level > 9) return false;
  if (write_buffer_size_mb == 0) return false;

  // Check TOF binning
  if (tof_binning.num_bins == 0) return false;
  if (tof_binning.range_seconds <= 0.0) return false;

  // Mode-specific checks
  if (isRadiographyMode() && max_neutrons_in_memory == 0) return false;
  if (temporal_window_ms < 0.0) return false;

  return true;
}

// JSON serialization
nlohmann::json StreamingConfig::toJson() const {
  nlohmann::json j;

  j["mode"] = getModeString();
  j["max_hits_in_memory"] = max_hits_in_memory;
  j["max_neutrons_in_memory"] = max_neutrons_in_memory;
  j["chunk_size_mb"] = chunk_size_mb;
  j["enable_tof_histograms"] = enable_tof_histograms;
  j["save_hits_to_disk"] = save_hits_to_disk;
  j["save_neutrons_to_disk"] = save_neutrons_to_disk;
  j["temporal_window_ms"] = temporal_window_ms;
  j["tiff_save_interval_sec"] = tiff_save_interval_sec;
  j["enable_progress_callbacks"] = enable_progress_callbacks;
  j["compression_level"] = compression_level;
  j["write_buffer_size_mb"] = write_buffer_size_mb;

  j["tof_binning"] = {{"num_bins", tof_binning.num_bins},
                      {"range_seconds", tof_binning.range_seconds}};

  return j;
}

StreamingConfig StreamingConfig::fromJson(const nlohmann::json& j) {
  StreamingConfig config;

  // Parse mode
  std::string mode_str = j.value("mode", "RADIOGRAPHY");
  if (mode_str == "IMAGING") {
    config.mode = ProcessingMode::IMAGING;
  } else if (mode_str == "RADIOGRAPHY") {
    config.mode = ProcessingMode::RADIOGRAPHY;
  } else {
    throw std::invalid_argument("Invalid processing mode: " + mode_str);
  }

  // Parse basic fields with defaults
  config.max_hits_in_memory = j.value("max_hits_in_memory", 50000000UL);
  config.max_neutrons_in_memory = j.value("max_neutrons_in_memory", 10000000UL);
  config.chunk_size_mb = j.value("chunk_size_mb", 512UL);
  config.enable_tof_histograms = j.value("enable_tof_histograms", false);
  config.save_hits_to_disk = j.value("save_hits_to_disk", false);
  config.save_neutrons_to_disk = j.value("save_neutrons_to_disk", true);
  config.temporal_window_ms = j.value("temporal_window_ms", 0.04);
  config.tiff_save_interval_sec = j.value("tiff_save_interval_sec", 30UL);
  config.enable_progress_callbacks = j.value("enable_progress_callbacks", true);
  config.compression_level = j.value("compression_level", 6);
  config.write_buffer_size_mb = j.value("write_buffer_size_mb", 100UL);

  // Parse TOF binning
  if (j.contains("tof_binning")) {
    auto tof_j = j["tof_binning"];
    config.tof_binning.num_bins = tof_j.value("num_bins", 1500UL);
    config.tof_binning.range_seconds = tof_j.value("range_seconds", 1.0 / 60.0);
  }

  // Validate configuration
  if (!config.isValid()) {
    throw std::invalid_argument("Invalid configuration values");
  }

  return config;
}

// File I/O
void StreamingConfig::saveToFile(const std::string& filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file for writing: " + filename);
  }

  nlohmann::json j = toJson();
  file << j.dump(2);
}

StreamingConfig StreamingConfig::fromFile(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file for reading: " + filename);
  }

  nlohmann::json j;
  try {
    file >> j;
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error("Invalid JSON in file " + filename + ": " +
                             e.what());
  }

  return fromJson(j);
}

}  // namespace tdcsophiread