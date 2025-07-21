// TDCSophiread ABS Clustering Performance Benchmark
// Validates optimization improvements for spatial indexing and memory pools

#include <gtest/gtest.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "neutron_processing/abs_clustering.h"
#include "neutron_processing/basic_neutron_processor.h"
#include "neutron_processing/neutron_factories.h"
#include "neutron_processing/neutron_processing.h"
#include "tdc_hit.h"

using namespace tdcsophiread;

class ABSPerformanceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Configure ABS clustering
    abs_config_.algorithm = "abs";
    abs_config_.abs.radius = 5.0;
    abs_config_.abs.min_cluster_size = 2;
    abs_config_.abs.neutron_correlation_window = 75.0;
    abs_config_.abs.scan_interval = 100;

    // Configure neutron processing
    processing_config_ = NeutronProcessingConfig::venusDefaults();
    processing_config_.clustering = abs_config_;
    processing_config_.temporal.num_workers =
        1;  // Single-threaded for this test
  }

  std::vector<TDCHit> createRealisticTestData(size_t num_hits,
                                              size_t detector_size = 256) {
    std::vector<TDCHit> hits;
    hits.reserve(num_hits);

    std::random_device rd;
    std::mt19937 gen(42);  // Fixed seed for reproducible benchmarks
    std::uniform_int_distribution<> pos_dist(10, detector_size - 10);
    std::uniform_int_distribution<> cluster_size_dist(1, 15);
    std::uniform_int_distribution<> offset_dist(-3, 3);
    std::uniform_int_distribution<> tof_dist(1000, 50000);
    std::uniform_int_distribution<> tot_dist(50, 200);
    std::uniform_int_distribution<> time_offset_dist(0, 50);

    // Create clusters
    size_t num_clusters = num_hits / 8;  // Average 8 hits per cluster
    size_t hit_count = 0;

    for (size_t cluster_id = 0;
         cluster_id < num_clusters && hit_count < num_hits; ++cluster_id) {
      // Random cluster center
      int center_x = pos_dist(gen);
      int center_y = pos_dist(gen);

      // Random cluster size
      size_t cluster_size = cluster_size_dist(gen);

      // Base timing for cluster
      uint32_t base_tof = tof_dist(gen);
      uint64_t base_timestamp = cluster_id * 1000 + time_offset_dist(gen);

      for (size_t hit_in_cluster = 0;
           hit_in_cluster < cluster_size && hit_count < num_hits;
           ++hit_in_cluster, ++hit_count) {
        TDCHit hit;

        // Spatial clustering around center
        hit.x = std::max(0, std::min(static_cast<int>(detector_size - 1),
                                     center_x + offset_dist(gen)));
        hit.y = std::max(0, std::min(static_cast<int>(detector_size - 1),
                                     center_y + offset_dist(gen)));

        // Temporal clustering within correlation window
        hit.tof = base_tof + time_offset_dist(gen);
        hit.timestamp = base_timestamp + hit_in_cluster * 10;
        hit.tot = tot_dist(gen);
        hit.chip_id = 0;

        hits.push_back(hit);
      }
    }

    // Add some random gamma noise (5%)
    size_t noise_hits = std::min(num_hits - hit_count, num_hits / 20);
    std::uniform_int_distribution<> noise_pos_dist(0, detector_size - 1);
    std::uniform_int_distribution<> noise_tot_dist(30, 100);

    for (size_t i = 0; i < noise_hits; ++i, ++hit_count) {
      TDCHit hit;
      hit.x = noise_pos_dist(gen);
      hit.y = noise_pos_dist(gen);
      hit.tof = tof_dist(gen);
      hit.timestamp = hit_count * 100 + time_offset_dist(gen);
      hit.tot = noise_tot_dist(gen);
      hit.chip_id = 0;
      hits.push_back(hit);
    }

    // Sort by timestamp to simulate acquisition order
    std::sort(hits.begin(), hits.end(), [](const TDCHit& a, const TDCHit& b) {
      return a.timestamp < b.timestamp;
    });

    return hits;
  }

  struct BenchmarkResult {
    std::string name;
    size_t hits_processed;
    size_t clusters_found;
    double time_ms;
    double hits_per_second;
    double efficiency_percent;
  };

  BenchmarkResult benchmarkClustering(const std::string& test_name,
                                      const std::vector<TDCHit>& hits) {
    SimpleABSClustering clusterer(abs_config_);

    // Warm up run to initialize memory pools
    clusterer.cluster(hits.begin(), hits.end());
    clusterer.reset();

    // Benchmark multiple runs
    const int num_runs = 5;
    std::vector<double> times;
    size_t clusters_found = 0;

    for (int run = 0; run < num_runs; ++run) {
      auto start = std::chrono::high_resolution_clock::now();

      clusterer.cluster(hits.begin(), hits.end());

      auto end = std::chrono::high_resolution_clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::microseconds>(end - start);
      times.push_back(duration.count() / 1000.0);  // Convert to ms

      if (run == 0) {
        clusters_found = clusterer.getStatistics().total_clusters;
      }

      clusterer.reset();
    }

    // Calculate average performance
    double avg_time_ms =
        std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    double hits_per_second = hits.size() / (avg_time_ms / 1000.0);
    double efficiency_percent =
        (static_cast<double>(clusters_found) / hits.size()) * 100.0;

    return {test_name,   hits.size(),     clusters_found,
            avg_time_ms, hits_per_second, efficiency_percent};
  }

  HitClusteringConfig abs_config_;
  NeutronProcessingConfig processing_config_;
};

// Test ABS clustering performance with various dataset sizes
TEST_F(ABSPerformanceTest, ClusteringPerformanceScaling) {
  std::cout << "\n=== ABS Clustering Performance Benchmark ===" << std::endl;
  std::cout << std::fixed << std::setprecision(2);

  std::vector<size_t> test_sizes = {1000, 5000, 10000, 25000};

  std::cout << std::left << std::setw(12) << "Dataset" << std::setw(12)
            << "Time (ms)" << std::setw(15) << "Hits/sec" << std::setw(10)
            << "Clusters" << std::setw(12) << "Efficiency" << std::endl;
  std::cout << std::string(65, '-') << std::endl;

  for (size_t size : test_sizes) {
    auto hits = createRealisticTestData(size);
    auto result = benchmarkClustering(std::to_string(size) + " hits", hits);

    std::cout << std::left << std::setw(12) << result.hits_processed
              << std::setw(12) << result.time_ms << std::setw(15)
              << static_cast<int>(result.hits_per_second) << std::setw(10)
              << result.clusters_found << std::setw(12)
              << result.efficiency_percent << "%" << std::endl;

    // Performance expectations
    EXPECT_GT(result.hits_per_second, 10000)
        << "Should process at least 10K hits/sec";
    EXPECT_GT(result.efficiency_percent, 5.0)
        << "Should find reasonable number of clusters";
  }

  std::cout << "\n✅ Optimizations implemented:" << std::endl;
  std::cout << "   • Spatial indexing: O(1) average bucket lookup" << std::endl;
  std::cout << "   • Memory pools: Pre-allocated vectors eliminate allocations"
            << std::endl;
  std::cout << "   • Efficient removal: Swap-and-pop for O(1) bucket removal"
            << std::endl;
  std::cout << "   • Zero-copy design: Iterator-based processing" << std::endl;
}

// Test memory usage characteristics
TEST_F(ABSPerformanceTest, MemoryPoolEfficiency) {
  std::cout << "\n=== Memory Pool Efficiency Test ===" << std::endl;

  // Test with repeated processing to verify pool reuse
  auto hits = createRealisticTestData(10000);
  SimpleABSClustering clusterer(abs_config_);

  // First run - establishes pool size
  auto start = std::chrono::high_resolution_clock::now();
  clusterer.cluster(hits.begin(), hits.end());
  auto first_time = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now() - start)
                        .count() /
                    1000.0;

  clusterer.reset();

  // Second run - should reuse pools
  start = std::chrono::high_resolution_clock::now();
  clusterer.cluster(hits.begin(), hits.end());
  auto second_time = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::high_resolution_clock::now() - start)
                         .count() /
                     1000.0;

  double pool_efficiency = (first_time - second_time) / first_time * 100.0;

  std::cout << "First run (pool allocation): " << first_time << " ms"
            << std::endl;
  std::cout << "Second run (pool reuse): " << second_time << " ms" << std::endl;
  std::cout << "Pool efficiency improvement: " << pool_efficiency << "%"
            << std::endl;

  // Second run should be at least as fast due to memory pools
  EXPECT_LE(second_time, first_time * 1.1)
      << "Pool reuse should not be significantly slower";
}

// Test temporal vs single-threaded performance comparison
TEST_F(ABSPerformanceTest, TemporalProcessingComparison) {
  std::cout << "\n=== Temporal vs Single-threaded Comparison ===" << std::endl;

  auto hits =
      createRealisticTestData(10000);  // Smaller dataset to avoid issues

  // Test just the basic neutron processor for comparison
  BasicNeutronProcessor single_processor(processing_config_);

  auto start = std::chrono::high_resolution_clock::now();
  auto single_neutrons = single_processor.processHits(hits.begin(), hits.end());
  auto single_time = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::high_resolution_clock::now() - start)
                         .count() /
                     1000.0;

  std::cout << "Single-threaded BasicNeutronProcessor: " << single_time
            << " ms, " << single_neutrons.size() << " neutrons" << std::endl;

  // Performance validation
  double hits_per_second = hits.size() / (single_time / 1000.0);
  std::cout << "Processing rate: " << static_cast<int>(hits_per_second)
            << " hits/sec" << std::endl;

  EXPECT_GT(hits_per_second, 5000) << "Should process at least 5K hits/sec";
  EXPECT_GT(single_neutrons.size(), 0) << "Should produce some neutrons";

  std::cout << "✅ Single-threaded processing validated" << std::endl;
  std::cout
      << "Note: Parallel comparison requires full temporal processor setup"
      << std::endl;
}

// Test spatial indexing effectiveness
TEST_F(ABSPerformanceTest, SpatialIndexingEffectiveness) {
  std::cout << "\n=== Spatial Indexing Effectiveness ===" << std::endl;

  // Create data with varying spatial density
  auto sparse_hits = createRealisticTestData(5000, 256);  // Normal density
  auto dense_hits = createRealisticTestData(5000, 128);   // Higher density

  auto sparse_result = benchmarkClustering("Sparse (256x256)", sparse_hits);
  auto dense_result = benchmarkClustering("Dense (128x128)", dense_hits);

  std::cout << "Sparse data: " << sparse_result.time_ms << " ms, "
            << static_cast<int>(sparse_result.hits_per_second) << " hits/sec"
            << std::endl;
  std::cout << "Dense data: " << dense_result.time_ms << " ms, "
            << static_cast<int>(dense_result.hits_per_second) << " hits/sec"
            << std::endl;

  double density_performance_ratio =
      dense_result.hits_per_second / sparse_result.hits_per_second;

  std::cout << "Dense/Sparse performance ratio: " << density_performance_ratio
            << std::endl;

  // Spatial indexing should maintain good performance even with higher density
  EXPECT_GT(density_performance_ratio, 0.5)
      << "Spatial indexing should handle dense data reasonably well";

  if (density_performance_ratio > 0.8) {
    std::cout << "✅ Excellent spatial indexing performance!" << std::endl;
  } else {
    std::cout << "✅ Acceptable spatial indexing performance" << std::endl;
  }
}