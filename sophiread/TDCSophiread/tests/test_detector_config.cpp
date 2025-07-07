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
  EXPECT_EQ(config.getChipSizeX(), 256);
  EXPECT_EQ(config.getChipSizeY(), 256);
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
        {"chip_layout", {{"chip_size_x", 256}, {"chip_size_y", 256}}}}}};

  std::string config_file = (test_dir / "test_config.json").string();
  createTestConfigFile("test_config.json", test_config);

  // Test loading from file
  auto config = DetectorConfig::fromFile(config_file);

  EXPECT_EQ(config.getTdcFrequency(), 50.0);
  EXPECT_FALSE(config.isMissingTdcCorrectionEnabled());
}

// Test 3: DetectorConfig should load from JSON object
TEST_F(TDCDetectorConfigTest, LoadsFromJsonObject) {
  // GREEN PHASE: Now testing actual implementation

  nlohmann::json config_json = {
      {"detector", {{"timing", {{"tdc_frequency_hz", 30.0}}}}}};

  // Test loading from JSON object
  auto config = DetectorConfig::fromJson(config_json);

  EXPECT_EQ(config.getTdcFrequency(), 30.0);

  // Verify other values remain as defaults
  EXPECT_TRUE(config.isMissingTdcCorrectionEnabled());
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

  // Test invalid configuration - zero chip size
  nlohmann::json invalid_config2 = {
      {"detector",
       {{"chip_layout", {{"chip_size_x", 0}}}}}};  // Invalid: zero chip size

  EXPECT_THROW(DetectorConfig::fromJson(invalid_config2),
               std::invalid_argument);
}

// Test 6: DetectorConfig should provide coordinate mapping functionality
TEST_F(TDCDetectorConfigTest, ProvidesCoordinateMapping) {
  // GREEN PHASE: Now testing actual implementation

  auto config = DetectorConfig::venusDefaults();

  // Test coordinate mapping using VENUS 2x2 layout with 2-pixel gaps
  // Updated from 5-pixel gaps to match current VENUS detector configuration

  auto [global_x0, global_y0] = config.mapChipToGlobal(0, 100, 150);
  EXPECT_EQ(global_x0, 100 + 258);  // Chip 0: x += 258
  EXPECT_EQ(global_y0, 150);        // y unchanged

  auto [global_x1, global_y1] = config.mapChipToGlobal(1, 100, 150);
  EXPECT_EQ(global_x1, 255 - 100 + 258);  // Chip 1: x = 255 - x + 258 = 413
  EXPECT_EQ(global_y1, 255 - 150 + 258);  // y = 255 - y + 258 = 363

  auto [global_x2, global_y2] = config.mapChipToGlobal(2, 100, 150);
  EXPECT_EQ(global_x2, 255 - 100);        // Chip 2: x = 255 - x = 155
  EXPECT_EQ(global_y2, 255 - 150 + 258);  // y = 255 - y + 258 = 363

  auto [global_x3, global_y3] = config.mapChipToGlobal(3, 100, 150);
  EXPECT_EQ(global_x3, 100);  // Chip 3: x unchanged (NO transformation)
  EXPECT_EQ(global_y3, 150);  // y unchanged
}

// Test 7: Should support JSON-configurable transformation matrices
TEST_F(TDCDetectorConfigTest, SupportsJsonTransformationMatrices) {
  // GREEN PHASE: Testing new matrix configuration capability

  // Create JSON configuration with custom transformation matrices
  nlohmann::json config_json = {
      {"detector",
       {{"timing",
         {{"tdc_frequency_hz", 40.0},
          {"enable_missing_tdc_correction", false}}},
        {"chip_layout", {{"chip_size_x", 512}, {"chip_size_y", 512}}},
        {"chip_transformations",
         {
             {{"chip_id", 0},
              {"matrix",
               {{2.0, 0.0, 100.0}, {0.0, 2.0, 50.0}}}},  // Scale by 2x + offset
             {{"chip_id", 1},
              {"matrix",
               {{1.0, 0.0, 512.0}, {0.0, 1.0, 0.0}}}},  // Translation only
             {{"chip_id", 2},
              {"matrix",
               {{0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}}}},  // 90° rotation (swap x,y)
             {{"chip_id", 3},
              {"matrix",
               {{-1.0, 0.0, 511.0}, {0.0, -1.0, 511.0}}}}  // 180° rotation
         }}}}};

  auto config = DetectorConfig::fromJson(config_json);

  // Verify timing parameters were loaded
  EXPECT_EQ(config.getTdcFrequency(), 40.0);
  EXPECT_FALSE(config.isMissingTdcCorrectionEnabled());

  // Verify chip parameters were loaded
  EXPECT_EQ(config.getChipSizeX(), 512);
  EXPECT_EQ(config.getChipSizeY(), 512);

  // Test transformation matrices work correctly
  // Test coordinates (10, 20)

  // Chip 0: Scale by 2x + offset -> (2*10+100, 2*20+50) = (120, 90)
  auto [x0, y0] = config.mapChipToGlobal(0, 10, 20);
  EXPECT_EQ(x0, 120);
  EXPECT_EQ(y0, 90);

  // Chip 1: Translation only -> (10+512, 20+0) = (522, 20)
  auto [x1, y1] = config.mapChipToGlobal(1, 10, 20);
  EXPECT_EQ(x1, 522);
  EXPECT_EQ(y1, 20);

  // Chip 2: 90° rotation (swap x,y) -> (0*10+1*20+0, 1*10+0*20+0) = (20, 10)
  auto [x2, y2] = config.mapChipToGlobal(2, 10, 20);
  EXPECT_EQ(x2, 20);
  EXPECT_EQ(y2, 10);

  // Chip 3: 180° rotation -> (-1*10+511, -1*20+511) = (501, 491)
  auto [x3, y3] = config.mapChipToGlobal(3, 10, 20);
  EXPECT_EQ(x3, 501);
  EXPECT_EQ(y3, 491);
}

// Test 7: Should provide pre-calculated TDC values for optimization
TEST_F(TDCDetectorConfigTest, ProvidesPreCalculatedTdcValues) {
  // Test VENUS defaults (60 Hz)
  auto venus_config = DetectorConfig::venusDefaults();

  // Verify TDC frequency
  EXPECT_DOUBLE_EQ(venus_config.getTdcFrequency(), 60.0);

  // Verify pre-calculated TDC period (should be 1.0 / 60.0)
  double expected_period = 1.0 / 60.0;
  EXPECT_DOUBLE_EQ(venus_config.getTdcPeriodSeconds(), expected_period);

  // Verify pre-calculated correction value (should match
  // applyMissingTDCCorrection calculation)
  uint32_t expected_correction =
      static_cast<uint32_t>(expected_period * 1e9 / 25 + 0.5);
  EXPECT_EQ(venus_config.getTdcCorrection25ns(), expected_correction);
  EXPECT_EQ(venus_config.getTdcCorrection25ns(),
            666667);  // Known value for 60Hz

  // Test different frequency via JSON
  nlohmann::json custom_config = {
      {"detector", {{"timing", {{"tdc_frequency_hz", 30.0}}}}}};

  auto config_30hz = DetectorConfig::fromJson(custom_config);

  // Verify 30Hz calculations
  EXPECT_DOUBLE_EQ(config_30hz.getTdcFrequency(), 30.0);
  EXPECT_DOUBLE_EQ(config_30hz.getTdcPeriodSeconds(), 1.0 / 30.0);

  uint32_t expected_correction_30hz =
      static_cast<uint32_t>((1.0 / 30.0) * 1e9 / 25 + 0.5);
  EXPECT_EQ(config_30hz.getTdcCorrection25ns(), expected_correction_30hz);
  EXPECT_EQ(config_30hz.getTdcCorrection25ns(),
            1333333);  // Known value for 30Hz
}

}  // namespace tdcsophiread