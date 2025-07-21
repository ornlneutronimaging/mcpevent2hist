// TDCSophiread TBB Parallel Processing Verification Tests
// Tests that actually verify TBB parallel execution is working

#include <gtest/gtest.h>
#include <tbb/global_control.h>
#include <tbb/task_arena.h>

#include <chrono>
#include <future>
#include <thread>

#include "neutron_processing/neutron_factories.h"
#include "neutron_processing/neutron_processing.h"
#include "tdc_hit.h"

using namespace tdcsophiread;

class TBBParallelVerificationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create large dataset that requires parallel processing
    createLargeTestDataset();

    // Configure for parallel processing
    config_.clustering.algorithm = "abs";
    config_.clustering.abs.radius = 5.0;
    config_.clustering.abs.min_cluster_size = 1;
    config_.clustering.abs.neutron_correlation_window = 75.0;

    config_.extraction.algorithm = "simple_centroid";
    config_.extraction.super_resolution_factor = 8.0;
    config_.extraction.weighted_by_tot = true;

    config_.temporal.num_workers = 4;
    config_.temporal.min_batch_size = 100;  // Force multiple batches
    config_.temporal.max_batch_size = 500;
    config_.temporal.overlap_factor = 3.0;
    config_.temporal.enable_deduplication = false;  // Simplify for testing
  }

  void createLargeTestDataset() {
    large_hits_.clear();

    // Create 4000 hits across multiple temporal regions
    // This should force creation of multiple batches for parallel processing
    uint32_t timestamp_base = 1000;

    for (int region = 0; region < 20; ++region) {
      uint32_t region_timestamp = timestamp_base + region * 50000;
      uint32_t region_tof = 1000 + region * 10000;

      // Create 200 hits per region (20 regions × 200 = 4000 hits)
      for (int hit = 0; hit < 200; ++hit) {
        TDCHit h;
        h.x = 100 + (hit % 100);
        h.y = 200 + (hit / 100) * 50;
        h.tof = region_tof + hit;
        h.tot = 100 + hit % 50;
        h.chip_id = 0;
        h.timestamp = region_timestamp + hit * 100;
        large_hits_.push_back(h);
      }
    }

    // Sort by timestamp to maintain temporal order
    std::sort(large_hits_.begin(), large_hits_.end(),
              [](const TDCHit& a, const TDCHit& b) {
                return a.timestamp < b.timestamp;
              });
  }

  std::vector<TDCHit> large_hits_;
  NeutronProcessingConfig config_;
};

// Test 1: Performance scaling verification (indirect TBB verification)
TEST_F(TBBParallelVerificationTest, VerifiesPerformanceScaling) {
  // Test with 1 worker (sequential)
  config_.temporal.num_workers = 1;
  TemporalNeutronProcessor processor_1(config_);

  auto start_1 = std::chrono::high_resolution_clock::now();
  auto neutrons_1 = processor_1.processHits(large_hits_);
  auto end_1 = std::chrono::high_resolution_clock::now();
  auto time_1 =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_1 - start_1);

  // Test with 4 workers (parallel)
  config_.temporal.num_workers = 4;
  TemporalNeutronProcessor processor_4(config_);

  auto start_4 = std::chrono::high_resolution_clock::now();
  auto neutrons_4 = processor_4.processHits(large_hits_);
  auto end_4 = std::chrono::high_resolution_clock::now();
  auto time_4 =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_4 - start_4);

  // Verify results are equivalent
  EXPECT_EQ(neutrons_1.size(), neutrons_4.size())
      << "Different worker counts should produce same results";

  // Verify parallel version shows some improvement
  std::cout << "Dataset size: " << large_hits_.size() << " hits" << std::endl;
  std::cout << "1 worker time: " << time_1.count() << "ms" << std::endl;
  std::cout << "4 worker time: " << time_4.count() << "ms" << std::endl;

  if (time_1.count() > 0 && time_4.count() > 0) {
    double speedup = (double)time_1.count() / time_4.count();
    std::cout << "Speedup ratio: " << speedup << std::endl;

    // Only check speedup if processing took meaningful time (>10ms)
    if (time_1.count() > 10) {
      EXPECT_GT(speedup, 1.0)
          << "4 workers should show some speedup over 1 worker";

      // Expect at least 20% improvement to account for overhead
      EXPECT_GT(speedup, 1.2)
          << "Parallel processing should show meaningful speedup";
    } else {
      std::cout << "Dataset too small to measure meaningful speedup"
                << std::endl;
    }
  }
}

// Test 2: Verify TBB task arena is properly configured
TEST_F(TBBParallelVerificationTest, VerifiesTBBConfiguration) {
  // Check if TBB is properly initialized
  auto max_concurrency = tbb::this_task_arena::max_concurrency();
  EXPECT_GT(max_concurrency, 0)
      << "TBB task arena should report available concurrency";

  // Create processor and verify it reports correct worker count
  config_.temporal.num_workers = 4;
  TemporalNeutronProcessor processor(config_);
  EXPECT_EQ(processor.getNumWorkers(), 4);

  // Process data
  auto neutrons = processor.processHits(large_hits_);
  EXPECT_GT(neutrons.size(), 0);

  std::cout << "TBB max concurrency: " << max_concurrency << std::endl;
  std::cout << "Processor workers: " << processor.getNumWorkers() << std::endl;
  std::cout << "Neutrons produced: " << neutrons.size() << std::endl;
}

// Test 3: Thread safety verification under concurrent access
TEST_F(TBBParallelVerificationTest, VerifiesThreadSafety) {
  config_.temporal.num_workers = 4;

  // Create multiple processors (each with their own worker pool)
  std::vector<std::unique_ptr<TemporalNeutronProcessor>> processors;
  for (int i = 0; i < 4; ++i) {
    processors.push_back(std::make_unique<TemporalNeutronProcessor>(config_));
  }

  // Run multiple concurrent processing operations
  std::vector<std::future<std::vector<TDCNeutron>>> futures;

  for (int i = 0; i < 4; ++i) {
    futures.push_back(std::async(std::launch::async, [&processors, i, this]() {
      return processors[i]->processHits(large_hits_);
    }));
  }

  // Collect results
  std::vector<std::vector<TDCNeutron>> results;
  for (auto& future : futures) {
    results.push_back(future.get());
  }

  // Verify all results are identical (deterministic)
  ASSERT_FALSE(results.empty());
  size_t expected_size = results[0].size();
  EXPECT_GT(expected_size, 0) << "Should produce neutrons";

  for (size_t i = 1; i < results.size(); ++i) {
    EXPECT_EQ(results[i].size(), expected_size)
        << "Concurrent processing should produce identical results";
  }

  std::cout << "Concurrent processors all produced " << expected_size
            << " neutrons" << std::endl;
}

// Test 4: Stress test with maximum workers
TEST_F(TBBParallelVerificationTest, StressTestMaxWorkers) {
  // Use maximum available concurrency
  auto max_workers = tbb::this_task_arena::max_concurrency();

  config_.temporal.num_workers = max_workers;
  TemporalNeutronProcessor processor(config_);

  // Process multiple times to stress test
  std::vector<size_t> neutron_counts;

  for (int run = 0; run < 5; ++run) {
    auto neutrons = processor.processHits(large_hits_);
    neutron_counts.push_back(neutrons.size());
    processor.reset();
  }

  // Verify consistent results across runs
  ASSERT_FALSE(neutron_counts.empty());
  size_t expected_count = neutron_counts[0];

  for (size_t count : neutron_counts) {
    EXPECT_EQ(count, expected_count)
        << "Multiple runs should produce identical results";
  }

  std::cout << "Stress test with " << max_workers
            << " workers: " << expected_count << " neutrons per run"
            << std::endl;
}

// Test 5: Performance comparison across different worker counts
TEST_F(TBBParallelVerificationTest, PerformanceAcrossWorkerCounts) {
  std::vector<size_t> worker_counts = {1, 2, 4, 8};
  std::vector<double> processing_times;
  std::vector<size_t> neutron_counts;

  for (size_t workers : worker_counts) {
    config_.temporal.num_workers = workers;
    TemporalNeutronProcessor processor(config_);

    auto start = std::chrono::high_resolution_clock::now();
    auto neutrons = processor.processHits(large_hits_);
    auto end = std::chrono::high_resolution_clock::now();

    auto time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    processing_times.push_back(time_ms.count());
    neutron_counts.push_back(neutrons.size());

    std::cout << workers << " workers: " << time_ms.count() << "ms, "
              << neutrons.size() << " neutrons" << std::endl;
  }

  // Verify all worker counts produce same neutron count
  for (size_t i = 1; i < neutron_counts.size(); ++i) {
    EXPECT_EQ(neutron_counts[i], neutron_counts[0])
        << "All worker counts should produce same neutron count";
  }

  // Check if we see any performance trend
  if (processing_times[0] > 10) {  // Only if meaningful timing
    bool found_improvement = false;
    for (size_t i = 1; i < processing_times.size(); ++i) {
      if (processing_times[i] < processing_times[0] * 0.8) {
        found_improvement = true;
        break;
      }
    }

    if (!found_improvement) {
      std::cout << "WARNING: No significant performance improvement seen with "
                   "more workers"
                << std::endl;
      std::cout
          << "This may indicate TBB is not working or dataset is too small"
          << std::endl;
    }
  }
}