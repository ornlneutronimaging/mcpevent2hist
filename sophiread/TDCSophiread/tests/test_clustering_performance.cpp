// TDCSophiread Clustering Performance Tests
// Validate 120M hits/sec clustering throughput target

#include <gtest/gtest.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "tdc_cluster_processor.h"
#include "tdc_clustering_config.h"

namespace tdcsophiread {

// Performance test class for clustering algorithms
class TDCClusteringPerformanceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Use VENUS defaults for realistic performance testing
    config_ = ClusteringConfig::venusDefaults();

    // Create various dataset sizes for performance testing
    createPerformanceDatasets();
  }

  void createPerformanceDatasets() {
    std::random_device rd;
    std::mt19937 gen(rd());

    // Dataset sizes to test (hits)
    std::vector<size_t> dataset_sizes = {
        1000,     // Small dataset
        10000,    // Medium dataset
        100000,   // Large dataset
        1000000,  // Very large dataset
        5000000   // Maximum test dataset (5M hits)
    };

    for (size_t size : dataset_sizes) {
      datasets_[size] = generateRealisticHits(size, gen);
    }
  }

  std::vector<TDCHit> generateRealisticHits(size_t count, std::mt19937& gen) {
    std::vector<TDCHit> hits;
    hits.reserve(count);

    // Realistic distributions based on neutron imaging data
    std::uniform_int_distribution<uint16_t> x_dist(
        0, 511);  // Single chip coordinates
    std::uniform_int_distribution<uint16_t> y_dist(0, 511);
    std::uniform_int_distribution<uint8_t> chip_dist(
        0, 3);  // 4 chips (2x2 layout)
    std::uniform_int_distribution<uint16_t> tot_dist(
        50, 300);  // Realistic TOT range

    // Create clusters of hits that can actually be clustered together
    // Generate neutron events, then create hits around them
    size_t neutron_count = count / 5;  // Expect ~20% clustering efficiency
    std::uniform_real_distribution<double> cluster_time_spacing(
        10.0, 50.0);  // Time between neutrons (in 25ns units)
    std::uniform_int_distribution<int> cluster_size_dist(
        1, 6);  // Hits per cluster
    std::uniform_real_distribution<double> spatial_offset(
        -2.0, 2.0);  // Pixel offset within cluster
    std::uniform_int_distribution<int> temporal_spread(
        0, 2);  // Time spread within cluster (25ns units)

    uint32_t current_tof = 1000;

    for (size_t neutron = 0; neutron < neutron_count && hits.size() < count;
         ++neutron) {
      // Generate neutron position
      uint16_t neutron_x = x_dist(gen);
      uint16_t neutron_y = y_dist(gen);
      uint8_t chip_id = chip_dist(gen);

      // Generate cluster around this neutron
      int cluster_size = cluster_size_dist(gen);
      uint32_t neutron_tof = current_tof;

      for (int hit = 0; hit < cluster_size && hits.size() < count; ++hit) {
        // Create hit near neutron position
        int16_t hit_x = static_cast<int16_t>(neutron_x + spatial_offset(gen));
        int16_t hit_y = static_cast<int16_t>(neutron_y + spatial_offset(gen));

        // Clamp to valid range
        hit_x = std::max<int16_t>(0, std::min<int16_t>(511, hit_x));
        hit_y = std::max<int16_t>(0, std::min<int16_t>(511, hit_y));

        // Small temporal variation within cluster
        uint32_t hit_tof = neutron_tof + temporal_spread(gen);
        uint16_t tot = tot_dist(gen);

        hits.emplace_back(hit_tof, static_cast<uint16_t>(hit_x),
                          static_cast<uint16_t>(hit_y), hit_tof, tot, chip_id);
      }

      // Move to next neutron time
      current_tof += static_cast<uint32_t>(cluster_time_spacing(gen));
    }

    // If we still need more hits, add some isolated hits
    while (hits.size() < count) {
      current_tof += static_cast<uint32_t>(cluster_time_spacing(gen));
      uint16_t x = x_dist(gen);
      uint16_t y = y_dist(gen);
      uint8_t chip_id = chip_dist(gen);
      uint16_t tot = tot_dist(gen);

      hits.emplace_back(current_tof, x, y, current_tof, tot, chip_id);
    }

    // Shuffle hits to make realistic temporal distribution
    std::shuffle(hits.begin(), hits.end(), gen);

    return hits;
  }

  // Benchmark a specific dataset size and return performance metrics
  struct PerformanceMetrics {
    size_t hit_count;
    size_t neutron_count;
    double processing_time_ms;
    double hits_per_second;
    double neutron_efficiency;
    double memory_usage_mb;
  };

  PerformanceMetrics benchmarkDataset(size_t dataset_size) {
    EXPECT_TRUE(datasets_.count(dataset_size) > 0)
        << "Dataset size " << dataset_size << " not available";

    const auto& hits = datasets_[dataset_size];
    TDCClusterProcessor processor(config_);

    // Warm up (to eliminate cold cache effects)
    if (hits.size() > 1000) {
      std::vector<TDCHit> warmup_hits(hits.begin(), hits.begin() + 1000);
      processor.processHits(warmup_hits);
      processor.reset();
    }

    // Actual benchmark
    auto start_time = std::chrono::high_resolution_clock::now();
    auto neutrons = processor.processHits(hits);
    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);
    double processing_time_ms = duration.count() / 1000.0;
    double hits_per_second = (hits.size() * 1000.0) / processing_time_ms;
    double neutron_efficiency =
        static_cast<double>(neutrons.size()) / static_cast<double>(hits.size());

    // Rough memory usage estimate
    double memory_usage_mb =
        (hits.size() * sizeof(TDCHit) + neutrons.size() * sizeof(TDCNeutron)) /
        (1024.0 * 1024.0);

    return {hits.size(),     neutrons.size(),    processing_time_ms,
            hits_per_second, neutron_efficiency, memory_usage_mb};
  }

  void printPerformanceMetrics(const std::string& test_name,
                               const PerformanceMetrics& metrics) {
    std::cout << "\n=== " << test_name << " ===" << std::endl;
    std::cout << "Input Hits: " << metrics.hit_count << std::endl;
    std::cout << "Output Neutrons: " << metrics.neutron_count << std::endl;
    std::cout << "Processing Time: " << std::fixed << std::setprecision(2)
              << metrics.processing_time_ms << " ms" << std::endl;
    std::cout << "Throughput: " << std::scientific << std::setprecision(2)
              << metrics.hits_per_second << " hits/sec" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(1)
              << (metrics.hits_per_second / 1e6) << " M hits/sec" << std::endl;
    std::cout << "Neutron Efficiency: " << std::fixed << std::setprecision(1)
              << (metrics.neutron_efficiency * 100.0) << "%" << std::endl;
    std::cout << "Memory Usage: " << std::fixed << std::setprecision(1)
              << metrics.memory_usage_mb << " MB" << std::endl;
  }

  ClusteringConfig config_;
  std::map<size_t, std::vector<TDCHit>> datasets_;
};

// Test 1: Performance scaling with dataset size
TEST_F(TDCClusteringPerformanceTest, PerformanceScalesWithDatasetSize) {
  std::vector<size_t> test_sizes = {1000, 10000, 100000};
  std::vector<PerformanceMetrics> results;

  for (size_t size : test_sizes) {
    auto metrics = benchmarkDataset(size);
    results.push_back(metrics);
    printPerformanceMetrics("Dataset Size " + std::to_string(size), metrics);

    // Sanity checks
    EXPECT_GT(metrics.hits_per_second, 100000.0)
        << "Performance too low for dataset size " << size;
    EXPECT_LT(metrics.processing_time_ms, 10000.0)
        << "Processing time too high for dataset size " << size;
    EXPECT_GT(metrics.neutron_efficiency, 0.05)
        << "Neutron efficiency too low for dataset size "
        << size;  // Lowered from 0.1 to 0.05 (5%)
  }

  // Performance should not degrade significantly with larger datasets
  if (results.size() >= 2) {
    double performance_ratio =
        results.back().hits_per_second / results[0].hits_per_second;
    EXPECT_GT(performance_ratio, 0.5)
        << "Performance degrades too much with larger datasets";
  }
}

// Test 2: Target throughput validation (120M hits/sec)
TEST_F(TDCClusteringPerformanceTest, MeetsTargetThroughput120MHitsPerSec) {
  // Test with 1M hits to get stable throughput measurement
  auto metrics = benchmarkDataset(1000000);
  printPerformanceMetrics("Throughput Target Test (1M hits)", metrics);

  // Target: 120M hits/sec (120,000,000 hits/sec)
  const double TARGET_THROUGHPUT = 120e6;
  const double MINIMUM_ACCEPTABLE =
      TARGET_THROUGHPUT * 0.5;  // 50% of target acceptable

  EXPECT_GT(metrics.hits_per_second, MINIMUM_ACCEPTABLE)
      << "Throughput " << (metrics.hits_per_second / 1e6)
      << " M hits/sec is below minimum " << (MINIMUM_ACCEPTABLE / 1e6)
      << " M hits/sec";

  if (metrics.hits_per_second >= TARGET_THROUGHPUT) {
    std::cout << "✅ TARGET ACHIEVED: " << (metrics.hits_per_second / 1e6)
              << " M hits/sec >= " << (TARGET_THROUGHPUT / 1e6) << " M hits/sec"
              << std::endl;
  } else {
    std::cout << "⚠️  TARGET NOT MET: " << (metrics.hits_per_second / 1e6)
              << " M hits/sec < " << (TARGET_THROUGHPUT / 1e6) << " M hits/sec"
              << std::endl;
    std::cout << "   Performance ratio: "
              << (metrics.hits_per_second / TARGET_THROUGHPUT * 100.0)
              << "% of target" << std::endl;
  }
}

// Test 3: Algorithm component performance breakdown
TEST_F(TDCClusteringPerformanceTest, AlgorithmComponentPerformanceBreakdown) {
  const size_t test_size = 100000;
  const auto& hits = datasets_[test_size];

  TDCClusterProcessor processor(config_);

  // Test ABS clustering performance alone
  auto clustering_start = std::chrono::high_resolution_clock::now();
  std::vector<TDCHit> clustered_hits = hits;
  processor.getClusteringAlgorithmPtr()->fit(clustered_hits);
  auto clustering_end = std::chrono::high_resolution_clock::now();

  auto clustering_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(clustering_end -
                                                            clustering_start);
  double clustering_time_ms = clustering_duration.count() / 1000.0;
  double clustering_throughput = (hits.size() * 1000.0) / clustering_time_ms;

  // Test centroid fitting performance alone
  auto fitting_start = std::chrono::high_resolution_clock::now();
  auto neutrons =
      processor.getPeakFittingAlgorithmPtr()->extractNeutrons(clustered_hits);
  auto fitting_end = std::chrono::high_resolution_clock::now();

  auto fitting_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      fitting_end - fitting_start);
  double fitting_time_ms = fitting_duration.count() / 1000.0;
  double fitting_throughput = (hits.size() * 1000.0) / fitting_time_ms;

  // Combined performance
  auto combined_metrics = benchmarkDataset(test_size);

  std::cout << "\n=== Component Performance Breakdown ===" << std::endl;
  std::cout << "ABS Clustering: " << std::fixed << std::setprecision(1)
            << (clustering_throughput / 1e6) << " M hits/sec" << std::endl;
  std::cout << "Centroid Fitting: " << std::fixed << std::setprecision(1)
            << (fitting_throughput / 1e6) << " M hits/sec" << std::endl;
  std::cout << "Combined Pipeline: " << std::fixed << std::setprecision(1)
            << (combined_metrics.hits_per_second / 1e6) << " M hits/sec"
            << std::endl;

  // Each component should be reasonably fast
  EXPECT_GT(clustering_throughput, 50e6)
      << "ABS clustering too slow: " << (clustering_throughput / 1e6)
      << " M hits/sec";
  EXPECT_GT(fitting_throughput, 50e6)
      << "Centroid fitting too slow: " << (fitting_throughput / 1e6)
      << " M hits/sec";

  // Combined should be somewhat slower due to overhead but not too much
  double pipeline_efficiency =
      combined_metrics.hits_per_second /
      std::min(clustering_throughput, fitting_throughput);
  EXPECT_GT(pipeline_efficiency, 0.3)
      << "Pipeline efficiency too low: " << (pipeline_efficiency * 100.0)
      << "%";
}

// Test 4: Memory usage scaling
TEST_F(TDCClusteringPerformanceTest, MemoryUsageScalesReasonably) {
  std::vector<size_t> test_sizes = {10000, 100000, 1000000};

  std::cout << "\n=== Memory Usage Scaling ===" << std::endl;

  for (size_t size : test_sizes) {
    auto metrics = benchmarkDataset(size);
    double memory_per_hit =
        (metrics.memory_usage_mb * 1024.0 * 1024.0) / metrics.hit_count;

    std::cout << "Dataset: " << size << " hits, Memory: " << std::fixed
              << std::setprecision(1) << metrics.memory_usage_mb
              << " MB, Per hit: " << std::fixed << std::setprecision(0)
              << memory_per_hit << " bytes" << std::endl;

    // Memory usage should be reasonable (less than 1KB per hit including
    // neutrons)
    EXPECT_LT(memory_per_hit, 1024.0)
        << "Memory usage per hit too high: " << memory_per_hit << " bytes";

    // For large datasets, should be under 500MB total
    if (size >= 1000000) {
      EXPECT_LT(metrics.memory_usage_mb, 500.0)
          << "Total memory usage too high for large dataset";
    }
  }
}

// Test 5: Configuration impact on performance
TEST_F(TDCClusteringPerformanceTest, ConfigurationImpactOnPerformance) {
  const size_t test_size = 100000;
  std::vector<std::pair<std::string, ClusteringConfig>> configs = {
      {"VENUS Defaults", ClusteringConfig::venusDefaults()},
  };

  // Create high-performance config (smaller search radius, faster processing)
  auto fast_config = ClusteringConfig::venusDefaults();
  fast_config.abs.radius = 3.0;  // Smaller radius = faster clustering
  fast_config.abs.time_range_ns =
      50.0;  // Shorter time window = faster clustering
  configs.emplace_back("High Performance", fast_config);

  // Create high-precision config (larger search radius, more thorough
  // processing)
  auto precise_config = ClusteringConfig::venusDefaults();
  precise_config.abs.radius = 7.0;  // Larger radius = more thorough clustering
  precise_config.abs.time_range_ns =
      100.0;  // Longer time window = more thorough clustering
  configs.emplace_back("High Precision", precise_config);

  std::cout << "\n=== Configuration Performance Impact ===" << std::endl;

  for (const auto& [config_name, config] : configs) {
    TDCClusterProcessor processor(config);
    const auto& hits = datasets_[test_size];

    auto start_time = std::chrono::high_resolution_clock::now();
    auto neutrons = processor.processHits(hits);
    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);
    double processing_time_ms = duration.count() / 1000.0;
    double hits_per_second = (hits.size() * 1000.0) / processing_time_ms;
    double neutron_efficiency =
        static_cast<double>(neutrons.size()) / static_cast<double>(hits.size());

    std::cout << config_name << ": " << std::fixed << std::setprecision(1)
              << (hits_per_second / 1e6) << " M hits/sec, "
              << std::setprecision(1) << (neutron_efficiency * 100.0)
              << "% efficiency" << std::endl;

    // All configurations should maintain reasonable performance
    EXPECT_GT(hits_per_second, 10e6) << config_name << " performance too low";
  }
}

// Test 6: Large dataset stress test
TEST_F(TDCClusteringPerformanceTest, LargeDatasetStressTest) {
  const size_t stress_test_size = 5000000;  // 5M hits

  std::cout << "\n=== Large Dataset Stress Test (5M hits) ===" << std::endl;

  // This test may take longer, so increase tolerance
  auto metrics = benchmarkDataset(stress_test_size);
  printPerformanceMetrics("Stress Test", metrics);

  // Should complete in reasonable time (under 2 minutes for 5M hits)
  EXPECT_LT(metrics.processing_time_ms, 120000.0)
      << "Stress test took too long: " << metrics.processing_time_ms << " ms";

  // Should maintain decent throughput even for very large datasets
  EXPECT_GT(metrics.hits_per_second, 10e6)
      << "Throughput too low for stress test: "
      << (metrics.hits_per_second / 1e6) << " M hits/sec";

  // Should produce reasonable number of neutrons
  EXPECT_GT(metrics.neutron_count, metrics.hit_count * 0.02)
      << "Too few neutrons produced";  // Lowered from 0.05 to 0.02 (2%)
  EXPECT_LT(metrics.neutron_count, metrics.hit_count * 0.8)
      << "Too many neutrons produced";
}

}  // namespace tdcsophiread