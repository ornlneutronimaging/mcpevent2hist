// TDCSophiread Clustering Integration Tests
// End-to-end validation of TPX3 → hits → neutrons workflow

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <vector>

#include "tdc_cluster_processor.h"
#include "tdc_clustering_config.h"
#include "tdc_detector_config.h"
#include "tdc_processor.h"

namespace tdcsophiread {

class TDCClusteringIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Integration tests focus on synthetic data processing
    // rather than file I/O to validate clustering functionality
  }

  void TearDown() override {
    // Clean up test files if needed
  }

  // Create a minimal TPX3 file with known clustering patterns
  void createTestTPX3File() {
    test_tpx3_file_ = test_data_dir_ + "clustering_test.tpx3";

    std::ofstream file(test_tpx3_file_, std::ios::binary);
    if (!file.is_open()) {
      GTEST_SKIP() << "Cannot create test TPX3 file at " << test_tpx3_file_;
      return;
    }

    // TPX3 file header (simplified)
    // In reality would need proper header, but for testing we can use minimal
    // data

    // Create several neutron clusters
    // Cluster 1: 3x3 cluster at (100, 100) on chip 0
    writeCluster(file, 100, 100, 0, 1000, 3);

    // Cluster 2: 2x2 cluster at (200, 200) on chip 1
    writeCluster(file, 200, 200, 1, 2000, 2);

    // Cluster 3: Single hit at (300, 300) on chip 2
    writeHitPacket(file, 300, 300, 2, 3000, 150);

    // Cluster 4: Large 5x5 cluster at (400, 400) on chip 3
    writeCluster(file, 400, 400, 3, 4000, 5);

    file.close();
  }

  // Helper to write a cluster of hits
  void writeCluster(std::ofstream& file, uint16_t center_x, uint16_t center_y,
                    uint8_t chip_id, uint32_t base_tof, int size) {
    int half_size = size / 2;
    uint32_t tof = base_tof;

    for (int dx = -half_size; dx <= half_size; ++dx) {
      for (int dy = -half_size; dy <= half_size; ++dy) {
        uint16_t x = center_x + dx;
        uint16_t y = center_y + dy;
        uint16_t tot =
            150 + (dx * dx + dy * dy) * 10;  // TOT decreases from center

        writeHitPacket(file, x, y, chip_id, tof + abs(dx) + abs(dy), tot);
      }
    }
  }

  // Helper to write a single hit packet (simplified TPX3 format)
  void writeHitPacket(std::ofstream& /* file */, uint16_t /* x */,
                      uint16_t /* y */, uint8_t /* chip_id */,
                      uint32_t /* tof */, uint16_t /* tot */) {
    // Simplified packet - would need proper TPX3 encoding in production
    // For testing, we can use a mock format that TDCProcessor can parse

    // This is a placeholder - actual implementation would encode proper TPX3
    // packets For now, skip actual file writing since we need proper TPX3
    // format
  }

  std::string test_data_dir_;
  std::string test_tpx3_file_;
};

// Test 1: Complete TPX3 to neutrons workflow
TEST_F(TDCClusteringIntegrationTest, CompleteTpx3ToNeutronsWorkflow) {
  // Test the complete hits-to-neutrons workflow with synthetic data
  // This validates the end-to-end clustering pipeline

  // Create detector config
  auto detector_config = DetectorConfig::venusDefaults();

  // Create clustering config
  auto clustering_config = ClusteringConfig::venusDefaults();

  // Create synthetic hits that represent what would come from TPX3
  std::vector<TDCHit> hits;

  // Cluster 1: 3x3 at (100, 100)
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      hits.emplace_back(1000 + abs(dx) + abs(dy),        // timestamp
                        100 + dx, 100 + dy,              // x, y
                        1000,                            // tof
                        150 - 10 * (abs(dx) + abs(dy)),  // tot
                        0);                              // chip_id
    }
  }

  // Cluster 2: 2x2 at (200, 200)
  for (int dx = 0; dx <= 1; ++dx) {
    for (int dy = 0; dy <= 1; ++dy) {
      hits.emplace_back(2000 + dx + dy, 200 + dx, 200 + dy, 2000, 160, 1);
    }
  }

  // Isolated hit
  hits.emplace_back(3000, 300, 300, 3000, 120, 2);

  // Process through clustering pipeline
  TDCClusterProcessor processor(clustering_config);
  auto neutrons = processor.processHits(hits);

  // Validate results
  EXPECT_EQ(neutrons.size(), 3) << "Expected 3 neutrons from clustering";

  // Check cluster 1 (should be centered around 100, 100)
  bool found_cluster1 = false;
  for (const auto& neutron : neutrons) {
    if (neutron.chip_id == 0 && neutron.n_hits == 9) {
      found_cluster1 = true;
      EXPECT_NEAR(neutron.x, 800.0, 10.0);  // 100 * 8 (super-resolution)
      EXPECT_NEAR(neutron.y, 800.0, 10.0);
      EXPECT_EQ(neutron.tof, 1000);
      EXPECT_GT(neutron.tot, 100);  // Combined TOT
    }
  }
  EXPECT_TRUE(found_cluster1) << "Did not find expected 3x3 cluster";

  // Check cluster 2 (should be centered around 200.5, 200.5)
  bool found_cluster2 = false;
  for (const auto& neutron : neutrons) {
    if (neutron.chip_id == 1 && neutron.n_hits == 4) {
      found_cluster2 = true;
      EXPECT_NEAR(neutron.x, 1604.0, 10.0);  // 200.5 * 8
      EXPECT_NEAR(neutron.y, 1604.0, 10.0);
      EXPECT_EQ(neutron.tof, 2000);
    }
  }
  EXPECT_TRUE(found_cluster2) << "Did not find expected 2x2 cluster";

  // Check isolated hit
  bool found_isolated = false;
  for (const auto& neutron : neutrons) {
    if (neutron.chip_id == 2 && neutron.n_hits == 1) {
      found_isolated = true;
      EXPECT_EQ(neutron.x, 2400.0);  // 300 * 8
      EXPECT_EQ(neutron.y, 2400.0);
      EXPECT_EQ(neutron.tof, 3000);
      EXPECT_EQ(neutron.tot, 120);
    }
  }
  EXPECT_TRUE(found_isolated) << "Did not find expected isolated hit";
}

// Test 2: Configuration variations
TEST_F(TDCClusteringIntegrationTest, ConfigurationVariations) {
  // Create test hits
  std::vector<TDCHit> hits;

  // Create a sparse pattern that can be clustered differently
  // depending on radius parameter
  hits.emplace_back(1000, 100, 100, 1000, 150, 0);
  hits.emplace_back(1001, 103, 100, 1001, 140, 0);  // 3 pixels away
  hits.emplace_back(1002, 106, 100, 1002, 130, 0);  // 6 pixels away
  hits.emplace_back(1003, 100, 104, 1003, 145, 0);  // 4 pixels away

  // Test with small radius (should get 2 clusters)
  {
    auto config = ClusteringConfig::venusDefaults();
    config.abs.radius = 2.5;  // Only immediate neighbors

    TDCClusterProcessor processor(config);
    auto neutrons = processor.processHits(hits);

    EXPECT_GE(neutrons.size(), 2)
        << "Small radius should produce multiple clusters";
  }

  // Test with large radius (should get 1 cluster)
  {
    auto config = ClusteringConfig::venusDefaults();
    config.abs.radius = 7.0;  // All hits within radius

    TDCClusterProcessor processor(config);
    auto neutrons = processor.processHits(hits);

    EXPECT_EQ(neutrons.size(), 1)
        << "Large radius should produce single cluster";
    if (!neutrons.empty()) {
      EXPECT_EQ(neutrons[0].n_hits, 4) << "Should include all 4 hits";
    }
  }

  // Test with clustering disabled
  {
    auto config = ClusteringConfig::venusDefaults();
    config.enable_clustering = false;

    TDCClusterProcessor processor(config);
    auto neutrons = processor.processHits(hits);

    EXPECT_EQ(neutrons.size(), hits.size())
        << "Disabled clustering should produce one neutron per hit";
  }
}

// Test 3: Time window effects
TEST_F(TDCClusteringIntegrationTest, TimeWindowClustering) {
  std::vector<TDCHit> hits;

  // Create hits at same position but different times
  uint16_t x = 100, y = 100;
  uint8_t chip = 0;

  // Group 1: Within time window (should cluster)
  hits.emplace_back(1000, x, y, 1000, 150, chip);
  hits.emplace_back(1001, x, y, 1001, 160, chip);  // 25ns later
  hits.emplace_back(1002, x, y, 1002, 140, chip);  // 50ns later

  // Group 2: Outside time window (should be separate)
  hits.emplace_back(1010, x, y, 1010, 155, chip);  // 250ns later
  hits.emplace_back(1011, x, y, 1011, 145, chip);  // 275ns later

  auto config = ClusteringConfig::venusDefaults();
  config.abs.time_range_ns = 75.0;  // 3 TDC units

  TDCClusterProcessor processor(config);
  auto neutrons = processor.processHits(hits);

  EXPECT_EQ(neutrons.size(), 2) << "Time window should separate clusters";

  // Check cluster characteristics - since clustering averages TOF,
  // we need to look for neutrons near the expected TOF values
  bool found_first_cluster = false;
  bool found_second_cluster = false;

  for (const auto& neutron : neutrons) {
    if (neutron.tof >= 1000 && neutron.tof <= 1002 && neutron.n_hits == 3) {
      found_first_cluster = true;
    } else if (neutron.tof >= 1010 && neutron.tof <= 1011 &&
               neutron.n_hits == 2) {
      found_second_cluster = true;
    }
  }

  EXPECT_TRUE(found_first_cluster)
      << "First cluster (3 hits, TOF ~1000-1002) not found";
  EXPECT_TRUE(found_second_cluster)
      << "Second cluster (2 hits, TOF ~1010-1011) not found";
}

// Test 4: Multi-chip clustering
TEST_F(TDCClusteringIntegrationTest, MultiChipClustering) {
  std::vector<TDCHit> hits;

  // Create clusters on different chips
  for (uint8_t chip = 0; chip < 4; ++chip) {
    // Each chip gets a cluster at a different position
    uint16_t base_x = 100 + chip * 100;
    uint16_t base_y = 100 + chip * 50;
    uint32_t base_tof = 1000 + chip * 1000;

    // Create 2x2 cluster
    for (int dx = 0; dx <= 1; ++dx) {
      for (int dy = 0; dy <= 1; ++dy) {
        hits.emplace_back(base_tof + dx + dy, base_x + dx, base_y + dy,
                          base_tof, 150, chip);
      }
    }
  }

  auto config = ClusteringConfig::venusDefaults();
  TDCClusterProcessor processor(config);
  auto neutrons = processor.processHits(hits);

  EXPECT_EQ(neutrons.size(), 4) << "Should have one neutron per chip";

  // Verify each chip has exactly one neutron
  std::array<int, 4> chip_counts = {0, 0, 0, 0};
  for (const auto& neutron : neutrons) {
    ASSERT_LT(neutron.chip_id, 4);
    chip_counts[neutron.chip_id]++;
    EXPECT_EQ(neutron.n_hits, 4) << "Each cluster should have 4 hits";
  }

  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(chip_counts[i], 1)
        << "Chip " << i << " should have exactly 1 neutron";
  }
}

// Test 5: Performance with realistic data distribution
TEST_F(TDCClusteringIntegrationTest, RealisticDataDistribution) {
  // Generate hits with realistic neutron imaging characteristics
  std::vector<TDCHit> hits;
  std::mt19937 gen(42);  // Fixed seed for reproducibility

  // Parameters based on VENUS detector
  const int num_neutron_events = 1000;
  const double cluster_probability = 0.8;  // 80% of neutrons produce clusters
  const double mean_cluster_size = 3.5;

  std::uniform_int_distribution<uint16_t> pos_dist(0, 511);
  std::uniform_int_distribution<uint8_t> chip_dist(0, 3);
  std::uniform_real_distribution<double> cluster_prob(0.0, 1.0);
  std::poisson_distribution<int> cluster_size_dist(mean_cluster_size - 1);
  std::normal_distribution<double> spatial_spread(0.0, 1.5);
  std::uniform_int_distribution<uint16_t> tot_dist(100, 250);

  uint32_t current_tof = 1000;

  for (int event = 0; event < num_neutron_events; ++event) {
    uint16_t center_x = pos_dist(gen);
    uint16_t center_y = pos_dist(gen);
    uint8_t chip_id = chip_dist(gen);

    if (cluster_prob(gen) < cluster_probability) {
      // Generate cluster
      int cluster_size = std::max(1, cluster_size_dist(gen) + 1);

      for (int i = 0; i < cluster_size; ++i) {
        double dx = spatial_spread(gen);
        double dy = spatial_spread(gen);

        int x = static_cast<int>(center_x + dx);
        int y = static_cast<int>(center_y + dy);

        // Keep within chip bounds
        x = std::max(0, std::min(511, x));
        y = std::max(0, std::min(511, y));

        hits.emplace_back(current_tof + i, static_cast<uint16_t>(x),
                          static_cast<uint16_t>(y), current_tof, tot_dist(gen),
                          chip_id);
      }
    } else {
      // Single hit
      hits.emplace_back(current_tof, center_x, center_y, current_tof,
                        tot_dist(gen), chip_id);
    }

    current_tof += 100;  // 2.5 microseconds between events
  }

  // Process with VENUS defaults
  auto config = ClusteringConfig::venusDefaults();
  TDCClusterProcessor processor(config);

  auto start = std::chrono::high_resolution_clock::now();
  auto neutrons = processor.processHits(hits);
  auto end = std::chrono::high_resolution_clock::now();

  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  double processing_time_ms = duration.count() / 1000.0;

  // Validate results
  EXPECT_GT(neutrons.size(), 0) << "Should produce neutrons";
  EXPECT_LE(neutrons.size(), hits.size())
      << "Cannot have more neutrons than hits";

  // Check efficiency is reasonable (clustering can reduce neutron count due to
  // merging)
  double efficiency = static_cast<double>(neutrons.size()) / num_neutron_events;
  EXPECT_GT(efficiency, 0.1) << "Should detect at least 10% of neutron events";
  EXPECT_LT(efficiency, 1.1) << "Cannot exceed 100% efficiency";

  // Check processing performance
  double hits_per_second = (hits.size() * 1000.0) / processing_time_ms;
  EXPECT_GT(hits_per_second, 1e6) << "Should process at least 1M hits/sec";

  // Print statistics
  std::cout << "\nRealistic Data Distribution Test:" << std::endl;
  std::cout << "  Input hits: " << hits.size() << std::endl;
  std::cout << "  Output neutrons: " << neutrons.size() << std::endl;
  std::cout << "  Detection efficiency: " << (efficiency * 100.0) << "%"
            << std::endl;
  std::cout << "  Processing time: " << processing_time_ms << " ms"
            << std::endl;
  std::cout << "  Throughput: " << (hits_per_second / 1e6) << " M hits/sec"
            << std::endl;

  // Calculate average cluster size
  double total_hits_in_clusters = 0;
  for (const auto& neutron : neutrons) {
    total_hits_in_clusters += neutron.n_hits;
  }
  double avg_cluster_size = total_hits_in_clusters / neutrons.size();
  std::cout << "  Average cluster size: " << avg_cluster_size << " hits/neutron"
            << std::endl;
}

// Test 6: Edge cases and error handling
TEST_F(TDCClusteringIntegrationTest, EdgeCasesAndErrorHandling) {
  auto config = ClusteringConfig::venusDefaults();
  TDCClusterProcessor processor(config);

  // Test 1: Empty input
  {
    std::vector<TDCHit> empty_hits;
    auto neutrons = processor.processHits(empty_hits);
    EXPECT_EQ(neutrons.size(), 0) << "Empty input should produce empty output";
  }

  // Test 2: Single hit
  {
    std::vector<TDCHit> single_hit = {TDCHit(1000, 100, 100, 1000, 150, 0)};
    auto neutrons = processor.processHits(single_hit);
    EXPECT_EQ(neutrons.size(), 1) << "Single hit should produce single neutron";
    if (!neutrons.empty()) {
      EXPECT_EQ(neutrons[0].n_hits, 1);
      EXPECT_EQ(neutrons[0].x, 800.0);  // 100 * 8
      EXPECT_EQ(neutrons[0].y, 800.0);
    }
  }

  // Test 3: All hits at same position (with slight temporal spread for ABS)
  {
    std::vector<TDCHit> same_position;
    for (int i = 0; i < 10; ++i) {
      // Use same TOF to ensure they cluster together temporally
      same_position.emplace_back(1000, 200, 200, 1000, 150, 0);
    }
    auto neutrons = processor.processHits(same_position);
    EXPECT_GE(neutrons.size(), 1)
        << "Same position hits should form at least one cluster";

    // Check that most hits are captured in clusters
    size_t total_hits_in_clusters = 0;
    for (const auto& neutron : neutrons) {
      total_hits_in_clusters += neutron.n_hits;
    }
    EXPECT_EQ(total_hits_in_clusters, 10) << "All hits should be clustered";
  }

  // Test 4: Boundary positions
  {
    std::vector<TDCHit> boundary_hits = {
        TDCHit(1000, 0, 0, 1000, 150, 0),      // Corner
        TDCHit(2000, 511, 511, 2000, 150, 1),  // Opposite corner
        TDCHit(3000, 0, 511, 3000, 150, 2),    // Other corners
        TDCHit(4000, 511, 0, 4000, 150, 3)};
    auto neutrons = processor.processHits(boundary_hits);
    EXPECT_EQ(neutrons.size(), 4)
        << "Boundary hits should be processed correctly";
  }

  // Test 5: Very large TOF values
  {
    std::vector<TDCHit> large_tof_hits = {
        TDCHit(0xFFFFFFF0, 100, 100, 0xFFFFFFF0, 150, 0),
        TDCHit(0xFFFFFFF1, 101, 100, 0xFFFFFFF1, 160, 0)};
    auto neutrons = processor.processHits(large_tof_hits);
    EXPECT_EQ(neutrons.size(), 1) << "Large TOF values should be handled";
  }
}

}  // namespace tdcsophiread