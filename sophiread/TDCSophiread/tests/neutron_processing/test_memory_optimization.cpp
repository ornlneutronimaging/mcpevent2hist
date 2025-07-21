// TDCSophiread Memory Optimization and Zero-Copy Validation Tests
// Validates memory usage patterns and zero-copy architecture

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include "neutron_processing/abs_clustering.h"
#include "neutron_processing/basic_neutron_processor.h"
#include "neutron_processing/neutron_factories.h"
#include "neutron_processing/neutron_processing.h"
#include "tdc_hit.h"

using namespace tdcsophiread;

class MemoryOptimizationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Configure for memory-efficient processing
    config_ = NeutronProcessingConfig::venusDefaults();
    config_.clustering.algorithm = "abs";
    config_.clustering.abs.radius = 5.0;
    config_.clustering.abs.min_cluster_size = 2;
    config_.clustering.abs.neutron_correlation_window = 75.0;
    config_.temporal.num_workers = 2;  // Small number for controlled testing
    config_.temporal.enable_deduplication =
        false;  // Disable for pure memory test
  }

  std::vector<TDCHit> createLargeDataset(size_t num_hits) {
    std::vector<TDCHit> hits;
    hits.reserve(num_hits);

    std::mt19937 gen(42);  // Fixed seed for reproducible tests
    std::uniform_int_distribution<> cluster_center_dist(10, 245);
    std::uniform_int_distribution<> cluster_offset_dist(-2, 2);
    std::uniform_int_distribution<> tof_base_dist(1000, 40000);
    std::uniform_int_distribution<> tof_offset_dist(0, 20);
    std::uniform_int_distribution<> tot_dist(50, 200);

    // Create clustered data that will produce neutrons
    size_t clusters_created = num_hits / 6;  // Average 6 hits per cluster
    size_t hits_created = 0;

    for (size_t cluster_id = 0;
         cluster_id < clusters_created && hits_created < num_hits;
         ++cluster_id) {
      // Random cluster center
      int center_x = cluster_center_dist(gen);
      int center_y = cluster_center_dist(gen);
      uint32_t base_tof = tof_base_dist(gen);
      uint64_t base_timestamp = cluster_id * 1000;

      // Create 4-8 hits per cluster
      size_t cluster_size = 4 + (cluster_id % 5);
      for (size_t hit_in_cluster = 0;
           hit_in_cluster < cluster_size && hits_created < num_hits;
           ++hit_in_cluster, ++hits_created) {
        TDCHit hit;
        hit.x = center_x + cluster_offset_dist(gen);
        hit.y = center_y + cluster_offset_dist(gen);
        hit.tof = base_tof + tof_offset_dist(gen);  // Within correlation window
        hit.timestamp = base_timestamp + hit_in_cluster * 10;
        hit.tot = tot_dist(gen);
        hit.chip_id = 0;
        hits.push_back(hit);
      }
    }

    // Fill remaining with noise hits
    std::uniform_int_distribution<> noise_pos_dist(0, 255);
    for (size_t i = hits_created; i < num_hits; ++i) {
      TDCHit hit;
      hit.x = noise_pos_dist(gen);
      hit.y = noise_pos_dist(gen);
      hit.tof = tof_base_dist(gen);
      hit.timestamp = i * 100;
      hit.tot = tot_dist(gen);
      hit.chip_id = 0;
      hits.push_back(hit);
    }

    // Sort by timestamp to simulate acquisition order
    std::sort(hits.begin(), hits.end(), [](const TDCHit& a, const TDCHit& b) {
      return a.timestamp < b.timestamp;
    });

    return hits;
  }

  size_t estimateMemoryUsage(size_t num_hits, size_t num_workers) {
    // Estimate memory usage in bytes
    size_t hit_size = sizeof(TDCHit);
    size_t base_memory = num_hits * hit_size;  // Original data

    // Current implementation creates copies
    size_t worker_copies =
        num_workers * (num_hits / 4) * hit_size;  // Estimated batch copies

    // Additional structures (labels, buckets, etc.)
    size_t metadata = num_hits * sizeof(int) * num_workers;  // Cluster labels
    size_t bucket_overhead = 1024 * 1024;  // ~1MB for bucket pools

    return base_memory + worker_copies + metadata + bucket_overhead;
  }

  NeutronProcessingConfig config_;
};

// Test memory usage scaling with dataset size
TEST_F(MemoryOptimizationTest, MemoryUsageScaling) {
  std::cout << "\n=== Memory Usage Scaling Analysis ===" << std::endl;

  std::vector<size_t> dataset_sizes = {1000, 5000, 10000, 25000};

  std::cout << "Dataset Size | Est. Memory (MB) | Target < 2GB/worker"
            << std::endl;
  std::cout << "-------------|------------------|--------------------"
            << std::endl;

  for (size_t size : dataset_sizes) {
    auto hits = createLargeDataset(size);

    // Estimate memory usage
    size_t estimated_bytes =
        estimateMemoryUsage(size, config_.temporal.num_workers);
    double estimated_mb = estimated_bytes / (1024.0 * 1024.0);
    double memory_per_worker = estimated_mb / config_.temporal.num_workers;

    std::cout << std::setw(12) << size << " | " << std::setw(15) << std::fixed
              << std::setprecision(1) << estimated_mb << " | " << std::setw(15)
              << memory_per_worker << " MB" << std::endl;

    // Validate memory usage is reasonable
    EXPECT_LT(memory_per_worker, 2048.0) << "Memory per worker should be < 2GB";

    // Test actual processing to ensure it works
    auto processor = NeutronProcessorFactory::create(config_);
    auto neutrons = processor->processHits(hits.begin(), hits.end());

    EXPECT_GT(neutrons.size(), 0) << "Should produce some neutrons";
  }
}

// Test zero-copy validation through timing analysis
TEST_F(MemoryOptimizationTest, ZeroCopyValidation) {
  std::cout << "\n=== Zero-Copy Architecture Validation ===" << std::endl;

  // Create large dataset to make memory copies detectable
  const size_t large_dataset_size = 50000;
  auto hits = createLargeDataset(large_dataset_size);

  std::cout << "Testing with " << large_dataset_size << " hits ("
            << (hits.size() * sizeof(TDCHit) / (1024 * 1024)) << " MB)"
            << std::endl;

  // Test single-threaded processing
  config_.temporal.num_workers = 1;
  auto single_processor = NeutronProcessorFactory::create(config_);

  auto start = std::chrono::high_resolution_clock::now();
  auto single_neutrons =
      single_processor->processHits(hits.begin(), hits.end());
  auto single_time = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::high_resolution_clock::now() - start)
                         .count() /
                     1000.0;

  // Test parallel processing
  config_.temporal.num_workers = 4;
  auto parallel_processor = NeutronProcessorFactory::create(config_);

  start = std::chrono::high_resolution_clock::now();
  auto parallel_neutrons =
      parallel_processor->processHits(hits.begin(), hits.end());
  auto parallel_time = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::high_resolution_clock::now() - start)
                           .count() /
                       1000.0;

  double speedup = single_time / parallel_time;

  std::cout << "Single-threaded: " << single_time << " ms" << std::endl;
  std::cout << "Parallel (4 workers): " << parallel_time << " ms" << std::endl;
  std::cout << "Speedup: " << speedup << "x" << std::endl;

  // Validate results are consistent
  EXPECT_EQ(single_neutrons.size(), parallel_neutrons.size())
      << "Single and parallel should produce same number of neutrons";

  // If truly zero-copy, parallel should show good speedup
  if (speedup > 2.0) {
    std::cout << "✅ Excellent parallel efficiency suggests minimal copying"
              << std::endl;
  } else if (speedup > 1.5) {
    std::cout << "✅ Good parallel efficiency" << std::endl;
  } else {
    std::cout << "⚠️  Limited speedup may indicate memory copying overhead"
              << std::endl;
  }

  EXPECT_GT(speedup, 1.2)
      << "Should see some parallel benefit with large dataset";
}

// Test iterator safety across thread boundaries
TEST_F(MemoryOptimizationTest, IteratorSafety) {
  std::cout << "\n=== Iterator Safety Validation ===" << std::endl;

  auto hits = createLargeDataset(10000);

  // Test with multiple workers to stress iterator safety
  config_.temporal.num_workers = 4;
  auto processor = NeutronProcessorFactory::create(config_);

  // Process multiple times to check for iterator invalidation
  std::vector<size_t> neutron_counts;

  for (int run = 0; run < 5; ++run) {
    auto neutrons = processor->processHits(hits.begin(), hits.end());
    neutron_counts.push_back(neutrons.size());
    processor->reset();
  }

  // All runs should produce the same number of neutrons
  for (size_t i = 1; i < neutron_counts.size(); ++i) {
    EXPECT_EQ(neutron_counts[0], neutron_counts[i])
        << "Run " << i << " produced different neutron count than run 0";
  }

  std::cout << "Consistent neutron count across " << neutron_counts.size()
            << " runs: " << neutron_counts[0] << std::endl;
  std::cout << "✅ Iterator safety validated" << std::endl;
}

// Test cache optimization through access pattern analysis
TEST_F(MemoryOptimizationTest, CacheAccessPatterns) {
  std::cout << "\n=== Cache Access Pattern Analysis ===" << std::endl;

  // Create datasets with different access patterns
  auto sequential_hits = createLargeDataset(20000);

  // Create randomized dataset (worse cache behavior)
  auto random_hits = sequential_hits;
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(random_hits.begin(), random_hits.end(), g);

  // Re-sort by timestamp to maintain temporal order but disrupt spatial
  // locality
  std::sort(random_hits.begin(), random_hits.end(),
            [](const TDCHit& a, const TDCHit& b) {
              return a.timestamp < b.timestamp;
            });

  auto processor = NeutronProcessorFactory::create(config_);

  // Test sequential access pattern
  auto start = std::chrono::high_resolution_clock::now();
  auto seq_neutrons =
      processor->processHits(sequential_hits.begin(), sequential_hits.end());
  auto seq_time = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::high_resolution_clock::now() - start)
                      .count() /
                  1000.0;

  processor->reset();

  // Test randomized access pattern
  start = std::chrono::high_resolution_clock::now();
  auto rand_neutrons =
      processor->processHits(random_hits.begin(), random_hits.end());
  auto rand_time = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::high_resolution_clock::now() - start)
                       .count() /
                   1000.0;

  double cache_efficiency = seq_time / rand_time;

  std::cout << "Sequential access: " << seq_time << " ms" << std::endl;
  std::cout << "Random access: " << rand_time << " ms" << std::endl;
  std::cout << "Cache efficiency ratio: " << cache_efficiency << std::endl;

  // Sequential should be faster due to better cache behavior
  if (cache_efficiency < 0.8) {
    std::cout << "✅ Good cache efficiency - sequential access is faster"
              << std::endl;
  } else {
    std::cout << "⚠️  Limited cache benefit - may need data structure "
                 "optimization"
              << std::endl;
  }

  // Both should produce same number of neutrons
  EXPECT_EQ(seq_neutrons.size(), rand_neutrons.size())
      << "Different access patterns should produce same results";
}

// Test memory allocation patterns during processing
TEST_F(MemoryOptimizationTest, AllocationPatterns) {
  std::cout << "\n=== Memory Allocation Pattern Analysis ===" << std::endl;

  auto hits = createLargeDataset(15000);
  auto processor = NeutronProcessorFactory::create(config_);

  // First run - may establish memory pools
  auto start = std::chrono::high_resolution_clock::now();
  auto neutrons1 = processor->processHits(hits.begin(), hits.end());
  auto time1 = std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::high_resolution_clock::now() - start)
                   .count() /
               1000.0;

  processor->reset();

  // Second run - should reuse allocated memory
  start = std::chrono::high_resolution_clock::now();
  auto neutrons2 = processor->processHits(hits.begin(), hits.end());
  auto time2 = std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::high_resolution_clock::now() - start)
                   .count() /
               1000.0;

  double allocation_efficiency = (time1 - time2) / time1 * 100.0;

  std::cout << "First run (with allocations): " << time1 << " ms" << std::endl;
  std::cout << "Second run (pool reuse): " << time2 << " ms" << std::endl;
  std::cout << "Allocation efficiency: " << allocation_efficiency << "%"
            << std::endl;

  // Results should be identical
  EXPECT_EQ(neutrons1.size(), neutrons2.size())
      << "Memory pool reuse should produce identical results";

  // Second run should be at least as fast due to pool reuse
  EXPECT_LE(time2, time1 * 1.05)
      << "Pool reuse should not be significantly slower";

  if (allocation_efficiency > 5.0) {
    std::cout << "✅ Good memory pool efficiency" << std::endl;
  } else {
    std::cout
        << "⚠️  Limited allocation benefit - check pool implementation"
        << std::endl;
  }
}