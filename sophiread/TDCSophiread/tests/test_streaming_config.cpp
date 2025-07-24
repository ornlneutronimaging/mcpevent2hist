// TDCSophiread StreamingConfig Tests
// TDD approach: Tests written FIRST to specify behavior

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "tdc_streaming_config.h"

using namespace tdcsophiread;
using json = nlohmann::json;

// Test fixture for StreamingConfig tests
class StreamingConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create temporary directory for test files
    test_dir = std::filesystem::temp_directory_path() / "streaming_config_test";
    std::filesystem::create_directories(test_dir);
  }

  void TearDown() override {
    // Clean up test files
    if (std::filesystem::exists(test_dir)) {
      std::filesystem::remove_all(test_dir);
    }
  }

  // Helper to create JSON config file
  void createConfigFile(const std::string& filename, const json& config) {
    std::ofstream file(test_dir / filename);
    file << config.dump(2);
  }

  // Helper to create invalid JSON file
  void createInvalidJsonFile(const std::string& filename) {
    std::ofstream file(test_dir / filename);
    file << "{ invalid json content }";
  }

  std::filesystem::path test_dir;
};

// =========================== DEFAULT CONFIGURATIONS
// ===========================

TEST_F(StreamingConfigTest, ImagingDefaultsAreCorrect) {
  // SPECIFY: Imaging mode should have optimized defaults for constant memory
  auto config = StreamingConfig::imagingDefaults();

  EXPECT_EQ(config.mode, ProcessingMode::IMAGING);
  EXPECT_TRUE(config.enable_tof_histograms);
  EXPECT_TRUE(config.save_hits_to_disk);  // Per design: default ON for imaging
  EXPECT_FALSE(config.save_neutrons_to_disk);  // No clustering in imaging mode

  // Memory settings should be conservative for imaging
  EXPECT_GT(config.max_hits_in_memory, 0);
  EXPECT_GT(config.chunk_size_mb, 0);

  // TOF binning should be configured
  EXPECT_GT(config.tof_binning.num_bins, 0);
  EXPECT_GT(config.tof_binning.range_seconds, 0.0);

  // TIFF save interval should be reasonable
  EXPECT_GT(config.tiff_save_interval_sec, 0);
}

TEST_F(StreamingConfigTest, RadiographyDefaultsAreCorrect) {
  // SPECIFY: Radiography mode should have optimized defaults for clustering
  auto config = StreamingConfig::radiographyDefaults();

  EXPECT_EQ(config.mode, ProcessingMode::RADIOGRAPHY);
  EXPECT_FALSE(config.enable_tof_histograms);  // No TOF accumulation
  EXPECT_FALSE(config.save_hits_to_disk);      // Default off per design
  EXPECT_TRUE(config.save_neutrons_to_disk);   // Primary output

  // Memory settings should allow larger buffers for clustering
  EXPECT_GT(config.max_hits_in_memory, 0);
  EXPECT_GT(config.max_neutrons_in_memory, 0);

  // Temporal window should be configured
  EXPECT_GT(config.temporal_window_ms, 0.0);

  // Progress reporting should be enabled
  EXPECT_TRUE(config.enable_progress_callbacks);
}

// =========================== MEMORY MANAGEMENT ===========================

TEST_F(StreamingConfigTest, MemoryLimitsAreReasonable) {
  // SPECIFY: Memory limits should be reasonable for typical systems
  auto imaging_config = StreamingConfig::imagingDefaults();
  auto radiography_config = StreamingConfig::radiographyDefaults();

  // Should not be too small (unusable) or too large (crashes)
  EXPECT_GE(imaging_config.max_hits_in_memory, 1000000);    // At least 1M hits
  EXPECT_LE(imaging_config.max_hits_in_memory, 100000000);  // At most 100M hits

  EXPECT_GE(radiography_config.max_neutrons_in_memory,
            100000);  // At least 100K neutrons
  EXPECT_LE(radiography_config.max_neutrons_in_memory,
            50000000);  // At most 50M neutrons

  // Chunk sizes should be reasonable
  EXPECT_GE(imaging_config.chunk_size_mb, 128);   // At least 128MB
  EXPECT_LE(imaging_config.chunk_size_mb, 2048);  // At most 2GB
}

TEST_F(StreamingConfigTest, CanCalculateMemoryUsage) {
  // SPECIFY: Should be able to estimate memory usage
  auto config = StreamingConfig::radiographyDefaults();

  size_t hit_memory = config.estimateHitMemoryUsage();
  size_t neutron_memory = config.estimateNeutronMemoryUsage();
  size_t total_memory = config.estimateTotalMemoryUsage();

  EXPECT_GT(hit_memory, 0);
  EXPECT_GT(neutron_memory, 0);
  EXPECT_EQ(total_memory, hit_memory + neutron_memory);

  // Memory estimates should be proportional to limits
  EXPECT_GT(hit_memory, neutron_memory);  // Hits typically use more memory
}

// =========================== TOF BINNING CONFIGURATION
// ===========================

TEST_F(StreamingConfigTest, TOFBinningIsConfiguredCorrectly) {
  // SPECIFY: TOF binning should have sensible defaults
  auto config = StreamingConfig::imagingDefaults();

  EXPECT_EQ(config.tof_binning.num_bins, 1500);  // Per design plan
  EXPECT_NEAR(config.tof_binning.range_seconds, 1.0 / 60.0,
              0.001);  // 60Hz default

  // Derived values should be correct
  double bin_width = config.tof_binning.getBinWidth();
  EXPECT_GT(bin_width, 0.0);
  EXPECT_NEAR(bin_width,
              config.tof_binning.range_seconds / config.tof_binning.num_bins,
              1e-9);
}

TEST_F(StreamingConfigTest, TOFBinningCanConvertTimes) {
  // SPECIFY: TOF binning should convert between time and bin indices
  TOFBinningConfig tof_config{1000, 0.1};  // 1000 bins, 100ms range

  // Test time to bin conversion
  EXPECT_EQ(tof_config.timeToBin(0.0), 0);
  EXPECT_EQ(tof_config.timeToBin(0.05), 500);   // Middle bin
  EXPECT_EQ(tof_config.timeToBin(0.099), 990);  // Near end (0.099/0.0001 = 990)
  EXPECT_EQ(tof_config.timeToBin(0.15), 999);   // Clamp to max

  // Test bin to time conversion
  EXPECT_NEAR(tof_config.binToTime(0), 0.0, 1e-9);
  EXPECT_NEAR(tof_config.binToTime(500), 0.05, 1e-6);
  EXPECT_NEAR(tof_config.binToTime(999), 0.0999, 1e-6);
}

// =========================== HDF5 CONFIGURATION ===========================

TEST_F(StreamingConfigTest, HDF5SettingsAreReasonable) {
  // SPECIFY: HDF5 settings should be optimized for performance
  auto config = StreamingConfig::radiographyDefaults();

  // Compression should be enabled but not too aggressive
  EXPECT_GE(config.compression_level, 0);
  EXPECT_LE(config.compression_level, 9);

  // Write buffer should be reasonable
  EXPECT_GE(config.write_buffer_size_mb, 10);    // At least 10MB
  EXPECT_LE(config.write_buffer_size_mb, 1000);  // At most 1GB
}

// =========================== JSON SERIALIZATION ===========================

TEST_F(StreamingConfigTest, CanConvertToJson) {
  // SPECIFY: Should be able to serialize config to JSON
  auto config = StreamingConfig::imagingDefaults();
  json j = config.toJson();

  // Check required fields are present
  EXPECT_TRUE(j.contains("mode"));
  EXPECT_TRUE(j.contains("max_hits_in_memory"));
  EXPECT_TRUE(j.contains("chunk_size_mb"));
  EXPECT_TRUE(j.contains("enable_tof_histograms"));
  EXPECT_TRUE(j.contains("tof_binning"));

  // Check TOF binning structure
  EXPECT_TRUE(j["tof_binning"].contains("num_bins"));
  EXPECT_TRUE(j["tof_binning"].contains("range_seconds"));

  // Values should match original config
  EXPECT_EQ(j["max_hits_in_memory"], config.max_hits_in_memory);
  EXPECT_EQ(j["tof_binning"]["num_bins"], config.tof_binning.num_bins);
}

TEST_F(StreamingConfigTest, CanLoadFromJson) {
  // SPECIFY: Should be able to deserialize config from JSON
  json j = {{"mode", "RADIOGRAPHY"},
            {"max_hits_in_memory", 25000000},
            {"max_neutrons_in_memory", 5000000},
            {"chunk_size_mb", 1024},
            {"enable_tof_histograms", false},
            {"save_hits_to_disk", true},
            {"save_neutrons_to_disk", true},
            {"temporal_window_ms", 0.05},
            {"compression_level", 7},
            {"write_buffer_size_mb", 150},
            {"tof_binning", {{"num_bins", 2000}, {"range_seconds", 0.02}}}};

  auto config = StreamingConfig::fromJson(j);

  EXPECT_EQ(config.mode, ProcessingMode::RADIOGRAPHY);
  EXPECT_EQ(config.max_hits_in_memory, 25000000);
  EXPECT_EQ(config.max_neutrons_in_memory, 5000000);
  EXPECT_EQ(config.chunk_size_mb, 1024);
  EXPECT_FALSE(config.enable_tof_histograms);
  EXPECT_TRUE(config.save_hits_to_disk);
  EXPECT_TRUE(config.save_neutrons_to_disk);
  EXPECT_NEAR(config.temporal_window_ms, 0.05, 1e-9);
  EXPECT_EQ(config.compression_level, 7);
  EXPECT_EQ(config.write_buffer_size_mb, 150);
  EXPECT_EQ(config.tof_binning.num_bins, 2000);
  EXPECT_NEAR(config.tof_binning.range_seconds, 0.02, 1e-9);
}

TEST_F(StreamingConfigTest, RoundTripJsonConversionWorks) {
  // SPECIFY: JSON serialization should be lossless
  auto original_config = StreamingConfig::radiographyDefaults();
  json j = original_config.toJson();
  auto restored_config = StreamingConfig::fromJson(j);

  EXPECT_EQ(original_config.mode, restored_config.mode);
  EXPECT_EQ(original_config.max_hits_in_memory,
            restored_config.max_hits_in_memory);
  EXPECT_EQ(original_config.max_neutrons_in_memory,
            restored_config.max_neutrons_in_memory);
  EXPECT_EQ(original_config.chunk_size_mb, restored_config.chunk_size_mb);
  EXPECT_EQ(original_config.enable_tof_histograms,
            restored_config.enable_tof_histograms);
  EXPECT_EQ(original_config.save_hits_to_disk,
            restored_config.save_hits_to_disk);
  EXPECT_EQ(original_config.save_neutrons_to_disk,
            restored_config.save_neutrons_to_disk);
  EXPECT_NEAR(original_config.temporal_window_ms,
              restored_config.temporal_window_ms, 1e-9);
  EXPECT_EQ(original_config.tof_binning.num_bins,
            restored_config.tof_binning.num_bins);
  EXPECT_NEAR(original_config.tof_binning.range_seconds,
              restored_config.tof_binning.range_seconds, 1e-9);
}

// =========================== FILE I/O ===========================

TEST_F(StreamingConfigTest, CanSaveToFile) {
  // SPECIFY: Should be able to save config to file
  auto config = StreamingConfig::imagingDefaults();
  auto config_path = test_dir / "test_config.json";

  config.saveToFile(config_path.string());

  EXPECT_TRUE(std::filesystem::exists(config_path));

  // Verify file content is valid JSON
  std::ifstream file(config_path);
  json j;
  EXPECT_NO_THROW(file >> j);

  // Should contain expected fields
  EXPECT_TRUE(j.contains("mode"));
  EXPECT_TRUE(j.contains("max_hits_in_memory"));
}

TEST_F(StreamingConfigTest, CanLoadFromFile) {
  // SPECIFY: Should be able to load config from file
  json config_json = {
      {"mode", "IMAGING"},
      {"max_hits_in_memory", 30000000},
      {"chunk_size_mb", 512},
      {"enable_tof_histograms", true},
      {"save_hits_to_disk", false},
      {"tiff_save_interval_sec", 45},
      {"tof_binning", {{"num_bins", 1200}, {"range_seconds", 0.025}}}};

  auto config_path = test_dir / "load_test.json";
  createConfigFile("load_test.json", config_json);

  auto config = StreamingConfig::fromFile(config_path.string());

  EXPECT_EQ(config.mode, ProcessingMode::IMAGING);
  EXPECT_EQ(config.max_hits_in_memory, 30000000);
  EXPECT_EQ(config.chunk_size_mb, 512);
  EXPECT_TRUE(config.enable_tof_histograms);
  EXPECT_FALSE(config.save_hits_to_disk);
  EXPECT_EQ(config.tiff_save_interval_sec, 45);
  EXPECT_EQ(config.tof_binning.num_bins, 1200);
  EXPECT_NEAR(config.tof_binning.range_seconds, 0.025, 1e-9);
}

// =========================== ERROR HANDLING ===========================

TEST_F(StreamingConfigTest, HandlesInvalidJsonGracefully) {
  // SPECIFY: Should handle invalid JSON files gracefully
  auto invalid_path = test_dir / "invalid.json";
  createInvalidJsonFile("invalid.json");

  EXPECT_THROW(StreamingConfig::fromFile(invalid_path.string()),
               std::runtime_error);
}

TEST_F(StreamingConfigTest, HandlesMissingFileGracefully) {
  // SPECIFY: Should handle missing files gracefully
  auto missing_path = test_dir / "nonexistent.json";

  EXPECT_THROW(StreamingConfig::fromFile(missing_path.string()),
               std::runtime_error);
}

TEST_F(StreamingConfigTest, HandlesMissingJsonFields) {
  // SPECIFY: Should handle missing fields by using defaults
  json incomplete_json = {
      {"mode", "IMAGING"}, {"max_hits_in_memory", 20000000}
      // Missing many required fields
  };

  // Should either throw or fill in reasonable defaults
  EXPECT_NO_THROW({
    auto config = StreamingConfig::fromJson(incomplete_json);
    EXPECT_EQ(config.mode, ProcessingMode::IMAGING);
    EXPECT_EQ(config.max_hits_in_memory, 20000000);
    // Other fields should have reasonable defaults
    EXPECT_GT(config.chunk_size_mb, 0);
  });
}

TEST_F(StreamingConfigTest, ValidatesConfigurationValues) {
  // SPECIFY: Should validate configuration values
  json invalid_json = {{"mode", "IMAGING"},
                       {"max_hits_in_memory", 0},  // Invalid: zero hits
                       {"chunk_size_mb", -100},    // Invalid: negative size
                       {"compression_level", 15},  // Invalid: > 9
                       {"tof_binning",
                        {
                            {"num_bins", 0},         // Invalid: zero bins
                            {"range_seconds", -1.0}  // Invalid: negative time
                        }}};

  EXPECT_THROW(StreamingConfig::fromJson(invalid_json), std::invalid_argument);
}

// =========================== UTILITY FUNCTIONS ===========================

TEST_F(StreamingConfigTest, CanDetectModeFromConfig) {
  // SPECIFY: Should be able to detect appropriate mode from settings
  auto imaging_config = StreamingConfig::imagingDefaults();
  auto radiography_config = StreamingConfig::radiographyDefaults();

  EXPECT_TRUE(imaging_config.isImagingMode());
  EXPECT_FALSE(imaging_config.isRadiographyMode());

  EXPECT_FALSE(radiography_config.isImagingMode());
  EXPECT_TRUE(radiography_config.isRadiographyMode());
}

TEST_F(StreamingConfigTest, CanGetModeString) {
  // SPECIFY: Should provide string representation of mode
  auto imaging_config = StreamingConfig::imagingDefaults();
  auto radiography_config = StreamingConfig::radiographyDefaults();

  EXPECT_EQ(imaging_config.getModeString(), "IMAGING");
  EXPECT_EQ(radiography_config.getModeString(), "RADIOGRAPHY");
}

TEST_F(StreamingConfigTest, CanValidateConfiguration) {
  // SPECIFY: Should be able to validate complete configuration
  auto valid_config = StreamingConfig::imagingDefaults();
  EXPECT_TRUE(valid_config.isValid());

  // Create invalid config
  StreamingConfig invalid_config = valid_config;
  invalid_config.max_hits_in_memory = 0;  // Invalid
  EXPECT_FALSE(invalid_config.isValid());
}

// =========================== PERFORMANCE SETTINGS ===========================

TEST_F(StreamingConfigTest, PerformanceSettingsAreOptimal) {
  // SPECIFY: Performance settings should be optimized for target systems
  auto config = StreamingConfig::radiographyDefaults();

  // Memory settings should target modern systems (8-32GB RAM)
  size_t total_memory_mb = config.estimateTotalMemoryUsage() / (1024 * 1024);
  EXPECT_GE(total_memory_mb, 500);   // At least 500MB
  EXPECT_LE(total_memory_mb, 8000);  // At most 8GB

  // Chunk size should be optimized for disk I/O
  EXPECT_GE(config.chunk_size_mb, 256);   // Large enough for efficient I/O
  EXPECT_LE(config.chunk_size_mb, 2048);  // Not too large for memory
}

TEST_F(StreamingConfigTest, CanCreateOptimizedConfigs) {
  // SPECIFY: Should provide configs optimized for different scenarios
  auto small_memory_config =
      StreamingConfig::forLimitedMemory(2048);  // 2GB limit
  auto large_memory_config =
      StreamingConfig::forHighThroughput();  // Optimized for speed

  // Small memory config should use less memory
  EXPECT_LT(small_memory_config.estimateTotalMemoryUsage(),
            large_memory_config.estimateTotalMemoryUsage());

  // Large memory config should use bigger chunks
  EXPECT_GT(large_memory_config.chunk_size_mb,
            small_memory_config.chunk_size_mb);
}