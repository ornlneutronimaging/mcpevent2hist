// TDCSophiread DetectorConfig Tests
// TDD approach: Tests written FIRST, then implementation

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "tdc_detector_config.h"

namespace tdcsophiread {

// Test class for DetectorConfig
class TDCDetectorConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create temporary directory for test files
    test_dir = std::filesystem::temp_directory_path() / "tdc_test_config";
    std::filesystem::create_directories(test_dir);
  }

  void TearDown() override {
    // Clean up test files
    if (std::filesystem::exists(test_dir)) {
      std::filesystem::remove_all(test_dir);
    }
  }

  // Helper to create test JSON files
  void createTestConfigFile(const std::string& filename,
                            const nlohmann::json& config) {
    std::ofstream file(test_dir / filename);
    file << config.dump(2);
    file.close();
  }

  std::filesystem::path test_dir;
};

// Test 1: DetectorConfig should have VENUS defaults
TEST_F(TDCDetectorConfigTest, HasVenusDefaults) {
  // GREEN PHASE: Now testing actual implementation
  auto config = DetectorConfig::venusDefaults();

  // Test VENUS default values from analysis document
  EXPECT_EQ(config.getTdcFrequency(), 60.0);
  EXPECT_TRUE(config.isMissingTdcCorrectionEnabled());
  EXPECT_EQ(config.getChipGapPixels(), 2);
  EXPECT_EQ(config.getChipsPerRow(), 2);
  EXPECT_EQ(config.getChipsPerCol(), 2);
  EXPECT_EQ(config.getChipSizeX(), 256);
  EXPECT_EQ(config.getChipSizeY(), 256);
  EXPECT_EQ(config.getSuperResolutionFactor(), 4);
}

// Test 2: DetectorConfig should load from JSON file
TEST_F(TDCDetectorConfigTest, LoadsFromValidJsonFile) {
  // GREEN PHASE: Now testing actual implementation

  // Create test JSON configuration
  nlohmann::json test_config = {
      {"detector",
       {{"name", "TEST_TPX3_2x2"},
        {"facility", "TEST"},
        {"timing",
         {{"tdc_frequency_hz", 50.0},
          {"enable_missing_tdc_correction", false}}},
        {"chip_layout",
         {{"chips_per_row", 2},
          {"chips_per_col", 2},
          {"chip_size_x", 256},
          {"chip_size_y", 256},
          {"chip_gap_pixels", 3}}},
        {"super_resolution", {{"factor", 4}, {"enable", true}}}}}};

  std::string config_file = (test_dir / "test_config.json").string();
  createTestConfigFile("test_config.json", test_config);

  // Test loading from file
  auto config = DetectorConfig::fromFile(config_file);

  EXPECT_EQ(config.getTdcFrequency(), 50.0);
  EXPECT_FALSE(config.isMissingTdcCorrectionEnabled());
  EXPECT_EQ(config.getChipGapPixels(), 3);
  EXPECT_EQ(config.getSuperResolutionFactor(), 4);
}

// Test 3: DetectorConfig should load from JSON object
TEST_F(TDCDetectorConfigTest, LoadsFromJsonObject) {
  // GREEN PHASE: Now testing actual implementation

  nlohmann::json config_json = {{"detector",
                                 {{"timing", {{"tdc_frequency_hz", 30.0}}},
                                  {"chip_layout", {{"chip_gap_pixels", 1}}}}}};

  // Test loading from JSON object
  auto config = DetectorConfig::fromJson(config_json);

  EXPECT_EQ(config.getTdcFrequency(), 30.0);
  EXPECT_EQ(config.getChipGapPixels(), 1);

  // Verify other values remain as defaults
  EXPECT_TRUE(config.isMissingTdcCorrectionEnabled());
  EXPECT_EQ(config.getChipsPerRow(), 2);
  EXPECT_EQ(config.getSuperResolutionFactor(), 4);
}

// Test 4: DetectorConfig should handle missing file gracefully
TEST_F(TDCDetectorConfigTest, HandlesMissingFileGracefully) {
  // GREEN PHASE: Now testing actual implementation

  std::string nonexistent_file = (test_dir / "nonexistent.json").string();

  // This should throw an exception
  EXPECT_THROW(DetectorConfig::fromFile(nonexistent_file), std::runtime_error);
}

// Test 5: DetectorConfig should validate configuration parameters
TEST_F(TDCDetectorConfigTest, ValidatesConfigurationParameters) {
  // GREEN PHASE: Now testing actual implementation

  // Test invalid configuration - negative frequency
  nlohmann::json invalid_config1 = {
      {"detector",
       {{"timing",
         {{"tdc_frequency_hz", -1.0}}}}}};  // Invalid: negative frequency

  EXPECT_THROW(DetectorConfig::fromJson(invalid_config1),
               std::invalid_argument);

  // Test invalid configuration - zero chips
  nlohmann::json invalid_config2 = {
      {"detector",
       {{"chip_layout", {{"chips_per_row", 0}}}}}};  // Invalid: zero chips

  EXPECT_THROW(DetectorConfig::fromJson(invalid_config2),
               std::invalid_argument);
}

// Test 6: DetectorConfig should provide coordinate mapping functionality
TEST_F(TDCDetectorConfigTest, ProvidesCoordinateMapping) {
  // GREEN PHASE: Now testing actual implementation

  auto config = DetectorConfig::venusDefaults();

  // Test coordinate mapping using exact transformations from Python reference
  // These match the hardcoded mappings in
  // Vlad_method_Bragg_edge_TDC_correction.ipynb

  auto [global_x0, global_y0] = config.mapChipToGlobal(0, 100, 150);
  EXPECT_EQ(global_x0, 100 + 260);  // Chip 0: x += 260
  EXPECT_EQ(global_y0, 150);        // y unchanged

  auto [global_x1, global_y1] = config.mapChipToGlobal(1, 100, 150);
  EXPECT_EQ(global_x1, 255 - 100 + 260);  // Chip 1: x = 255 - x + 260 = 415
  EXPECT_EQ(global_y1, 255 - 150 + 260);  // y = 255 - y + 260 = 365

  auto [global_x2, global_y2] = config.mapChipToGlobal(2, 100, 150);
  EXPECT_EQ(global_x2, 255 - 100);        // Chip 2: x = 255 - x = 155
  EXPECT_EQ(global_y2, 255 - 150 + 260);  // y = 255 - y + 260 = 365

  auto [global_x3, global_y3] = config.mapChipToGlobal(3, 100, 150);
  EXPECT_EQ(global_x3, 100);  // Chip 3: x unchanged (NO transformation)
  EXPECT_EQ(global_y3, 150);  // y unchanged

  // Test invalid chip ID
  EXPECT_THROW(config.mapChipToGlobal(4, 100, 150), std::invalid_argument);

  // Test out of bounds coordinates
  EXPECT_THROW(config.mapChipToGlobal(0, 256, 150), std::invalid_argument);
  EXPECT_THROW(config.mapChipToGlobal(0, 100, 256), std::invalid_argument);
}

}  // namespace tdcsophiread