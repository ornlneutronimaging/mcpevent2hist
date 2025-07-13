// TDCSophiread Clustering Configuration Tests
// TDD approach: Tests for clustering configuration system

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "tdc_clustering_config.h"

namespace tdcsophiread {

// Test class for ClusteringConfig
class TDCClusteringConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create temporary directory for test files
    test_dir =
        std::filesystem::temp_directory_path() / "tdc_test_clustering_config";
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

// Test 1: ClusteringConfig should have VENUS defaults
TEST_F(TDCClusteringConfigTest, HasVenusDefaults) {
  auto config = ClusteringConfig::venusDefaults();

  // Test algorithm selection
  EXPECT_EQ(config.clustering_algorithm, "abs");
  EXPECT_EQ(config.peak_fitting_algorithm, "centroid");
  EXPECT_TRUE(config.enable_clustering);
  EXPECT_FALSE(config.enable_performance_logging);

  // Test ABS defaults (VENUS optimized)
  EXPECT_EQ(config.abs.radius, 5.0);
  EXPECT_EQ(config.abs.min_cluster_size, 1);
  EXPECT_EQ(config.abs.neutron_correlation_window, 75.0);
  EXPECT_EQ(config.abs.scan_interval, static_cast<size_t>(100));

  // Test Centroid defaults (8x super-resolution)
  EXPECT_EQ(config.centroid.super_resolution_factor, 8.0);
  EXPECT_TRUE(config.centroid.weighted_by_tot);
  EXPECT_EQ(config.centroid.min_tot_threshold, 0.0);

  // Test FastGaussian defaults
  EXPECT_EQ(config.fastgaussian.super_resolution_factor, 8.0);
  EXPECT_EQ(config.fastgaussian.min_cluster_size, 8);
  EXPECT_EQ(config.fastgaussian.tot_filter_fraction, 0.5);
}

// Test 2: ClusteringConfig should load from JSON file
TEST_F(TDCClusteringConfigTest, LoadsFromValidJsonFile) {
  // Create test JSON configuration
  nlohmann::json test_config = {
      {"clustering",
       {{"clustering_algorithm", "abs"},
        {"peak_fitting_algorithm", "fastgaussian"},
        {"enable_clustering", true},
        {"enable_performance_logging", true},
        {"abs",
         {{"radius", 3.0}, {"min_cluster_size", 2}, {"time_range_ns", 50.0}}},
        {"centroid",
         {{"super_resolution_factor", 4.0}, {"weighted_by_tot", false}}},
        {"fastgaussian",
         {{"super_resolution_factor", 16.0}, {"min_cluster_size", 10}}}}}};

  std::string config_file = (test_dir / "test_clustering_config.json").string();
  createTestConfigFile("test_clustering_config.json", test_config);

  // Test loading from file
  auto config = ClusteringConfig::fromFile(config_file);

  EXPECT_EQ(config.clustering_algorithm, "abs");
  EXPECT_EQ(config.peak_fitting_algorithm, "fastgaussian");
  EXPECT_TRUE(config.enable_clustering);
  EXPECT_TRUE(config.enable_performance_logging);

  // Test ABS parameters were loaded
  EXPECT_EQ(config.abs.radius, 3.0);
  EXPECT_EQ(config.abs.min_cluster_size, 2);
  EXPECT_EQ(config.abs.neutron_correlation_window, 50.0);

  // Test Centroid parameters were loaded
  EXPECT_EQ(config.centroid.super_resolution_factor, 4.0);
  EXPECT_FALSE(config.centroid.weighted_by_tot);

  // Test FastGaussian parameters were loaded
  EXPECT_EQ(config.fastgaussian.super_resolution_factor, 16.0);
  EXPECT_EQ(config.fastgaussian.min_cluster_size, 10);
}

// Test 3: ClusteringConfig should load from JSON object
TEST_F(TDCClusteringConfigTest, LoadsFromJsonObject) {
  nlohmann::json config_json = {
      {"clustering",
       {{"clustering_algorithm", "abs"},
        {"abs", {{"radius", 7.5}, {"time_range_ns", 100.0}}}}}};

  // Test loading from JSON object
  auto config = ClusteringConfig::fromJson(config_json);

  EXPECT_EQ(config.clustering_algorithm, "abs");
  EXPECT_EQ(config.abs.radius, 7.5);
  EXPECT_EQ(config.abs.neutron_correlation_window, 100.0);

  // Verify other values remain as defaults
  EXPECT_EQ(config.peak_fitting_algorithm, "centroid");
  EXPECT_EQ(config.abs.min_cluster_size, 1);  // Default value
}

// Test 4: ClusteringConfig should handle missing file gracefully
TEST_F(TDCClusteringConfigTest, HandlesMissingFileGracefully) {
  std::string nonexistent_file = (test_dir / "nonexistent.json").string();

  // This should throw an exception
  EXPECT_THROW(ClusteringConfig::fromFile(nonexistent_file),
               std::runtime_error);
}

// Test 5: ClusteringConfig should validate configuration parameters
TEST_F(TDCClusteringConfigTest, ValidatesConfigurationParameters) {
  // Test invalid clustering algorithm
  nlohmann::json invalid_config1 = {
      {"clustering", {{"clustering_algorithm", "invalid_algorithm"}}}};

  EXPECT_THROW(ClusteringConfig::fromJson(invalid_config1),
               std::invalid_argument);

  // Test invalid peak fitting algorithm
  nlohmann::json invalid_config2 = {
      {"clustering", {{"peak_fitting_algorithm", "invalid_fitting"}}}};

  EXPECT_THROW(ClusteringConfig::fromJson(invalid_config2),
               std::invalid_argument);

  // Test invalid ABS parameters
  nlohmann::json invalid_config3 = {
      {"clustering", {{"abs", {{"radius", -1.0}}}}}};  // Negative radius

  EXPECT_THROW(ClusteringConfig::fromJson(invalid_config3),
               std::invalid_argument);
}

// Test 6: ClusteringConfig should support algorithm queries
TEST_F(TDCClusteringConfigTest, SupportsAlgorithmQueries) {
  // Test supported clustering algorithms
  auto clustering_algorithms =
      ClusteringConfig::getSupportedClusteringAlgorithms();
  EXPECT_FALSE(clustering_algorithms.empty());
  EXPECT_TRUE(ClusteringConfig::isClusteringAlgorithmSupported("abs"));
  EXPECT_FALSE(ClusteringConfig::isClusteringAlgorithmSupported("unknown"));

  // Test supported peak fitting algorithms
  auto peak_fitting_algorithms =
      ClusteringConfig::getSupportedPeakFittingAlgorithms();
  EXPECT_FALSE(peak_fitting_algorithms.empty());
  EXPECT_TRUE(ClusteringConfig::isPeakFittingAlgorithmSupported("centroid"));
  EXPECT_TRUE(
      ClusteringConfig::isPeakFittingAlgorithmSupported("fastgaussian"));
  EXPECT_FALSE(ClusteringConfig::isPeakFittingAlgorithmSupported("unknown"));
}

// Test 7: ClusteringConfig should generate JSON correctly
TEST_F(TDCClusteringConfigTest, GeneratesJsonCorrectly) {
  auto config = ClusteringConfig::venusDefaults();

  // Modify some parameters
  config.abs.radius = 6.0;
  config.centroid.super_resolution_factor = 10.0;
  config.enable_performance_logging = true;

  // Convert to JSON
  auto json = config.toJson();

  // Verify JSON structure
  EXPECT_TRUE(json.contains("clustering"));
  EXPECT_TRUE(json["clustering"].contains("clustering_algorithm"));
  EXPECT_TRUE(json["clustering"].contains("abs"));
  EXPECT_TRUE(json["clustering"].contains("centroid"));

  // Verify values
  EXPECT_EQ(json["clustering"]["clustering_algorithm"], "abs");
  EXPECT_EQ(json["clustering"]["abs"]["radius"], 6.0);
  EXPECT_EQ(json["clustering"]["centroid"]["super_resolution_factor"], 10.0);
  EXPECT_TRUE(json["clustering"]["enable_performance_logging"]);
}

// Test 8: ClusteringConfig should merge configurations
TEST_F(TDCClusteringConfigTest, MergesConfigurationsCorrectly) {
  auto config1 = ClusteringConfig::venusDefaults();
  auto config2 = ClusteringConfig::venusDefaults();

  // Modify config2
  config2.clustering_algorithm = "abs";
  config2.abs.radius = 10.0;
  config2.enable_performance_logging = true;

  // Merge config2 into config1
  config1.merge(config2);

  // Verify merge results
  EXPECT_EQ(config1.clustering_algorithm, "abs");
  EXPECT_EQ(config1.abs.radius, 10.0);
  EXPECT_TRUE(config1.enable_performance_logging);
}

// Test 9: ClusteringConfig should generate summary
TEST_F(TDCClusteringConfigTest, GeneratesSummary) {
  auto config = ClusteringConfig::venusDefaults();
  std::string summary = config.summary();

  // Verify summary contains key information
  EXPECT_TRUE(summary.find("Clustering Algorithm: abs") != std::string::npos);
  EXPECT_TRUE(summary.find("Peak Fitting Algorithm: centroid") !=
              std::string::npos);
  EXPECT_TRUE(summary.find("Radius: 5") != std::string::npos);
  EXPECT_TRUE(summary.find("Super Resolution: 8") != std::string::npos);
}

// Test 10: ABSConfig should validate parameters
TEST_F(TDCClusteringConfigTest, ABSConfigValidatesParameters) {
  ABSConfig config;

  // Test valid configuration
  config.radius = 5.0;
  config.min_cluster_size = 1;
  config.neutron_correlation_window = 75.0;
  config.scan_interval = 100;

  EXPECT_NO_THROW(config.validate());

  // Test invalid configurations
  config.radius = -1.0;  // Negative radius
  EXPECT_THROW(config.validate(), std::invalid_argument);

  config.radius = 5.0;
  config.min_cluster_size = 0;  // Zero cluster size
  EXPECT_THROW(config.validate(), std::invalid_argument);

  config.min_cluster_size = 1;
  config.neutron_correlation_window = -10.0;  // Negative time range
  EXPECT_THROW(config.validate(), std::invalid_argument);

  config.neutron_correlation_window = 75.0;
  config.scan_interval = 0;  // Zero scan interval
  EXPECT_THROW(config.validate(), std::invalid_argument);
}

// Test 11: CentroidConfig should validate parameters
TEST_F(TDCClusteringConfigTest, CentroidConfigValidatesParameters) {
  CentroidConfig config;

  // Test valid configuration
  config.super_resolution_factor = 8.0;
  config.weighted_by_tot = true;
  config.min_tot_threshold = 0.0;

  EXPECT_NO_THROW(config.validate());

  // Test invalid configurations
  config.super_resolution_factor = -1.0;  // Negative factor
  EXPECT_THROW(config.validate(), std::invalid_argument);

  config.super_resolution_factor = 8.0;
  config.min_tot_threshold = -5.0;  // Negative threshold
  EXPECT_THROW(config.validate(), std::invalid_argument);
}

// Test 12: FastGaussianConfig should validate parameters
TEST_F(TDCClusteringConfigTest, FastGaussianConfigValidatesParameters) {
  FastGaussianConfig config;

  // Test valid configuration
  config.super_resolution_factor = 8.0;
  config.min_cluster_size = 8;
  config.tot_filter_fraction = 0.5;
  config.max_iterations = 100;
  config.convergence_tolerance = 1e-6;

  EXPECT_NO_THROW(config.validate());

  // Test invalid configurations
  config.super_resolution_factor = 0.0;  // Zero factor
  EXPECT_THROW(config.validate(), std::invalid_argument);

  config.super_resolution_factor = 8.0;
  config.min_cluster_size = 2;  // Too small for FastGaussian
  EXPECT_THROW(config.validate(), std::invalid_argument);

  config.min_cluster_size = 8;
  config.tot_filter_fraction = 1.5;  // Invalid fraction (>1.0)
  EXPECT_THROW(config.validate(), std::invalid_argument);

  config.tot_filter_fraction = 0.5;
  config.max_iterations = 0;  // Zero iterations
  EXPECT_THROW(config.validate(), std::invalid_argument);

  config.max_iterations = 100;
  config.convergence_tolerance = -1.0;  // Negative tolerance
  EXPECT_THROW(config.validate(), std::invalid_argument);
}

}  // namespace tdcsophiread