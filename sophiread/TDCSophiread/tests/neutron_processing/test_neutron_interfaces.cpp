// TDCSophiread Neutron Processing Interface Tests
// Tests for factory creation, configuration management, and reset functionality

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "neutron_processing/neutron_config.h"
#include "neutron_processing/neutron_factories.h"
#include "tdc_hit.h"

namespace tdcsophiread {

/**
 * @brief Test fixture for neutron processing interface testing
 */
class NeutronInterfacesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test configuration with VENUS defaults
    config_ = NeutronProcessingConfig::venusDefaults();

    // Create test hits for interface validation
    test_hits_ = createTestHits();
  }

  // Helper to create standard test data
  std::vector<TDCHit> createTestHits() {
    std::vector<TDCHit> hits;

    // Create a simple 2x2 cluster of hits
    hits.push_back(createHit(100, 100, 1000, 150, 0));
    hits.push_back(createHit(101, 100, 1001, 120, 0));
    hits.push_back(createHit(100, 101, 1002, 140, 0));
    hits.push_back(createHit(101, 101, 1003, 130, 0));

    // Add an isolated hit (should form separate cluster)
    hits.push_back(createHit(200, 200, 1100, 110, 0));

    return hits;
  }

  TDCHit createHit(uint16_t x, uint16_t y, uint32_t tof, uint16_t tot = 100,
                   uint8_t chip_id = 0) {
    TDCHit hit;
    hit.x = x;
    hit.y = y;
    hit.tof = tof;
    hit.tot = tot;
    hit.chip_id = chip_id;
    hit.timestamp = tof;  // Simple case: TOF = timestamp
    return hit;
  }

  NeutronProcessingConfig config_;
  std::vector<TDCHit> test_hits_;
};

// Test 1: Hit Clustering Factory Tests
TEST_F(NeutronInterfacesTest, HitClusteringFactoryCreation) {
  // Test creating ABS clustering
  auto clusterer = HitClusteringFactory::create("abs", config_.clustering);
  ASSERT_NE(clusterer, nullptr);
  EXPECT_EQ(clusterer->getName(), "abs");

  // Test configuration access
  const auto& retrieved_config = clusterer->getConfig();
  EXPECT_EQ(retrieved_config.algorithm, "abs");
}

TEST_F(NeutronInterfacesTest, HitClusteringFactoryInvalidAlgorithm) {
  // Test invalid algorithm name
  EXPECT_THROW(
      HitClusteringFactory::create("invalid_algorithm", config_.clustering),
      std::invalid_argument);
}

// Test 2: Neutron Extraction Factory Tests
TEST_F(NeutronInterfacesTest, NeutronExtractionFactoryCreation) {
  // Test creating simple centroid extraction
  auto extractor =
      NeutronExtractionFactory::create("simple_centroid", config_.extraction);
  ASSERT_NE(extractor, nullptr);
  EXPECT_EQ(extractor->getName(), "simple_centroid");

  // Test configuration access
  const auto& retrieved_config = extractor->getConfig();
  EXPECT_EQ(retrieved_config.algorithm, "simple_centroid");
}

TEST_F(NeutronInterfacesTest, NeutronExtractionFactoryInvalidAlgorithm) {
  // Test invalid algorithm name
  EXPECT_THROW(NeutronExtractionFactory::create("invalid_extraction",
                                                config_.extraction),
               std::invalid_argument);
}

// Test 3: Neutron Processor Factory Tests
TEST_F(NeutronInterfacesTest, NeutronProcessorFactoryCreation) {
  // Test creating basic neutron processor
  auto processor = NeutronProcessorFactory::create(config_);
  ASSERT_NE(processor, nullptr);

  // Test algorithm names are correctly reported
  EXPECT_EQ(processor->getHitClusteringAlgorithm(), "abs");
  EXPECT_EQ(processor->getNeutronExtractionAlgorithm(), "simple_centroid");
}

// Test 4: Configuration Management Tests
TEST_F(NeutronInterfacesTest, HitClusteringConfigurationManagement) {
  auto clusterer = HitClusteringFactory::create("abs", config_.clustering);

  // Test initial configuration
  const auto& initial_config = clusterer->getConfig();
  EXPECT_EQ(initial_config.algorithm, "abs");

  // Test configuration update
  HitClusteringConfig new_config = config_.clustering;
  new_config.abs.radius = 10.0;  // Change radius

  clusterer->configure(new_config);
  const auto& updated_config = clusterer->getConfig();
  EXPECT_EQ(updated_config.abs.radius, 10.0);
}

TEST_F(NeutronInterfacesTest, NeutronExtractionConfigurationManagement) {
  auto extractor =
      NeutronExtractionFactory::create("simple_centroid", config_.extraction);

  // Test initial configuration
  const auto& initial_config = extractor->getConfig();
  EXPECT_EQ(initial_config.algorithm, "simple_centroid");

  // Test configuration update
  NeutronExtractionConfig new_config = config_.extraction;
  new_config.super_resolution_factor = 16.0;  // Change super resolution

  extractor->configure(new_config);
  const auto& updated_config = extractor->getConfig();
  EXPECT_EQ(updated_config.super_resolution_factor, 16.0);
}

// Test 5: Reset Functionality Tests
TEST_F(NeutronInterfacesTest, HitClusteringReset) {
  auto clusterer = HitClusteringFactory::create("abs", config_.clustering);

  // Create state and cluster labels for stateless operation
  auto state = clusterer->createState();
  std::vector<int> cluster_labels(test_hits_.size(), -1);

  // Process some hits
  size_t num_clusters = clusterer->cluster(test_hits_.begin(), test_hits_.end(),
                                           *state, cluster_labels);
  EXPECT_GT(num_clusters, 0);

  // Check that some hits were clustered
  int clustered_count = 0;
  for (int label : cluster_labels) {
    if (label >= 0) clustered_count++;
  }
  EXPECT_GT(clustered_count, 0);

  // State can be reset for reuse
  state->reset();

  // Configuration should be accessible
  const auto& config_after = clusterer->getConfig();
  EXPECT_EQ(config_after.algorithm, "abs");
}

TEST_F(NeutronInterfacesTest, NeutronExtractionReset) {
  auto clusterer = HitClusteringFactory::create("abs", config_.clustering);
  auto extractor =
      NeutronExtractionFactory::create("simple_centroid", config_.extraction);

  // Process hits through clustering first
  auto state = clusterer->createState();
  std::vector<int> cluster_labels(test_hits_.size(), -1);
  clusterer->cluster(test_hits_.begin(), test_hits_.end(), *state,
                     cluster_labels);

  // Extract neutrons
  auto neutrons =
      extractor->extract(test_hits_.begin(), test_hits_.end(), cluster_labels);
  EXPECT_GT(neutrons.size(), 0);

  // Reset should clear statistics but preserve configuration
  extractor->reset();

  // Configuration should be preserved
  const auto& config_after_reset = extractor->getConfig();
  EXPECT_EQ(config_after_reset.algorithm, "simple_centroid");
}

// Test 6: Interface Contract Tests
TEST_F(NeutronInterfacesTest, HitClusteringInterfaceContract) {
  auto clusterer = HitClusteringFactory::create("abs", config_.clustering);

  // Create state for testing
  auto state = clusterer->createState();

  // Test empty input
  std::vector<TDCHit> empty_hits;
  std::vector<int> empty_labels;
  size_t num_clusters = clusterer->cluster(empty_hits.begin(), empty_hits.end(),
                                           *state, empty_labels);
  EXPECT_EQ(num_clusters, 0);
  EXPECT_EQ(empty_labels.size(), 0);

  // Test single hit
  std::vector<TDCHit> single_hit = {createHit(100, 100, 1000)};
  std::vector<int> single_labels(1, -1);
  state->reset();
  num_clusters = clusterer->cluster(single_hit.begin(), single_hit.end(),
                                    *state, single_labels);
  EXPECT_GE(num_clusters,
            0);  // May or may not form cluster depending on min_cluster_size
  EXPECT_EQ(single_labels.size(), 1);

  // Test multiple hits
  std::vector<int> multi_labels(test_hits_.size(), -1);
  state->reset();
  num_clusters = clusterer->cluster(test_hits_.begin(), test_hits_.end(),
                                    *state, multi_labels);
  EXPECT_GT(num_clusters, 0);
  EXPECT_EQ(multi_labels.size(), test_hits_.size());
}

TEST_F(NeutronInterfacesTest, NeutronExtractionInterfaceContract) {
  auto clusterer = HitClusteringFactory::create("abs", config_.clustering);
  auto extractor =
      NeutronExtractionFactory::create("simple_centroid", config_.extraction);

  // Cluster the hits first
  auto state = clusterer->createState();
  std::vector<int> cluster_labels(test_hits_.size(), -1);
  size_t num_clusters = clusterer->cluster(test_hits_.begin(), test_hits_.end(),
                                           *state, cluster_labels);

  // Test extraction
  auto neutrons =
      extractor->extract(test_hits_.begin(), test_hits_.end(), cluster_labels);

  // Basic contracts
  EXPECT_LE(neutrons.size(),
            num_clusters);  // Can't have more neutrons than clusters

  // Each neutron should have valid properties
  for (const auto& neutron : neutrons) {
    EXPECT_GE(neutron.x, 0.0);
    EXPECT_GE(neutron.y, 0.0);
    EXPECT_GT(neutron.n_hits, 0);
    EXPECT_GT(neutron.tot, 0);
  }
}

// Test 7: Iterator Safety Tests
TEST_F(NeutronInterfacesTest, IteratorSafety) {
  auto clusterer = HitClusteringFactory::create("abs", config_.clustering);

  // Test const iterator usage (should not modify original hits)
  std::vector<TDCHit> original_hits = test_hits_;
  auto state2 = clusterer->createState();
  std::vector<int> labels2(test_hits_.size(), -1);
  clusterer->cluster(test_hits_.begin(), test_hits_.end(), *state2, labels2);

  // Original hits should be unchanged (for const iterator interface)
  EXPECT_EQ(test_hits_.size(), original_hits.size());
  for (size_t i = 0; i < test_hits_.size(); ++i) {
    EXPECT_EQ(test_hits_[i].x, original_hits[i].x);
    EXPECT_EQ(test_hits_[i].y, original_hits[i].y);
    EXPECT_EQ(test_hits_[i].tof, original_hits[i].tof);
    EXPECT_EQ(test_hits_[i].tot, original_hits[i].tot);
  }
}

}  // namespace tdcsophiread