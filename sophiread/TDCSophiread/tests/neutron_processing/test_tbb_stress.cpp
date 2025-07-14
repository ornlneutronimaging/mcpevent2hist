// TDCSophiread TBB Stress Test - Force meaningful processing time
// Tests with very large datasets to expose TBB behavior

#include <gtest/gtest.h>
#include <tbb/task_arena.h>

#include <chrono>

#include "neutron_processing/neutron_processing.h"
#include "tdc_hit.h"

using namespace tdcsophiread;

class TBBStressTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Configure for parallel processing
    config_.clustering.algorithm = "abs";
    config_.clustering.abs.radius = 5.0;
    config_.clustering.abs.min_cluster_size = 1;
    config_.clustering.abs.neutron_correlation_window = 75.0;

    config_.extraction.algorithm = "simple_centroid";
    config_.extraction.super_resolution_factor = 8.0;
    config_.extraction.weighted_by_tot = true;

    config_.temporal.min_batch_size = 1000;  // Large batches
    config_.temporal.max_batch_size = 5000;
    config_.temporal.overlap_factor = 3.0;
    config_.temporal.enable_deduplication = false;
  }

  std::vector<TDCHit> createMassiveDataset(size_t num_hits) {
    std::vector<TDCHit> hits;
    hits.reserve(num_hits);

    uint32_t timestamp_base = 1000;

    for (size_t i = 0; i < num_hits; ++i) {
      TDCHit h;
      h.x = 100 + (i % 1000);           // Spread across 1000 x positions
      h.y = 200 + ((i / 1000) % 1000);  // Spread across 1000 y positions
      h.tof = 1000 + (i % 50000);       // Spread across TOF range
      h.tot = 100 + (i % 200);          // Spread across TOT range
      h.chip_id = 0;
      h.timestamp = timestamp_base + i * 10;
      hits.push_back(h);
    }

    // Sort by timestamp to maintain temporal order
    std::sort(hits.begin(), hits.end(), [](const TDCHit& a, const TDCHit& b) {
      return a.timestamp < b.timestamp;
    });

    return hits;
  }

  NeutronProcessingConfig config_;
};

// Test with extremely large dataset to force measurable processing time
TEST_F(TBBStressTest, HugeDatasetStressTest) {
  // Create 100K hits to force meaningful processing time
  auto huge_hits = createMassiveDataset(100000);

  std::cout << "Created dataset with " << huge_hits.size() << " hits"
            << std::endl;

  // Test 1 worker
  config_.temporal.num_workers = 1;
  TemporalNeutronProcessor processor_1(config_);

  auto start_1 = std::chrono::high_resolution_clock::now();
  auto neutrons_1 = processor_1.processHits(huge_hits);
  auto end_1 = std::chrono::high_resolution_clock::now();
  auto time_1 =
      std::chrono::duration_cast<std::chrono::microseconds>(end_1 - start_1);

  // Test maximum workers (12 cores)
  auto max_workers = tbb::this_task_arena::max_concurrency();
  config_.temporal.num_workers = max_workers;
  TemporalNeutronProcessor processor_max(config_);

  auto start_max = std::chrono::high_resolution_clock::now();
  auto neutrons_max = processor_max.processHits(huge_hits);
  auto end_max = std::chrono::high_resolution_clock::now();
  auto time_max = std::chrono::duration_cast<std::chrono::microseconds>(
      end_max - start_max);

  std::cout << "1 worker: " << time_1.count() << "μs ("
            << time_1.count() / 1000.0 << "ms), " << neutrons_1.size()
            << " neutrons" << std::endl;
  std::cout << max_workers << " workers: " << time_max.count() << "μs ("
            << time_max.count() / 1000.0 << "ms), " << neutrons_max.size()
            << " neutrons" << std::endl;

  if (time_1.count() > 0 && time_max.count() > 0) {
    double speedup = (double)time_1.count() / time_max.count();
    std::cout << "Speedup with " << max_workers << " cores: " << speedup << "x"
              << std::endl;

    EXPECT_EQ(neutrons_1.size(), neutrons_max.size())
        << "Same dataset should produce same neutron count";

    // Record speedup for analysis (no hard threshold to avoid unstable tests)
    if (time_1.count() > 10000) {  // If > 10ms
      if (speedup > 1.1) {
        std::cout << "GOOD: Significant speedup detected" << std::endl;
      } else if (speedup > 1.0) {
        std::cout << "MODEST: Some speedup detected" << std::endl;
      } else {
        std::cout << "INFO: No speedup - may be memory bandwidth limited"
                  << std::endl;
      }
    }
  }
}

// Test smaller but still meaningful dataset
TEST_F(TBBStressTest, MediumDatasetPerformanceTest) {
  // Create 20K hits
  auto medium_hits = createMassiveDataset(20000);

  std::cout << "Created dataset with " << medium_hits.size() << " hits"
            << std::endl;

  // Test 1 worker
  config_.temporal.num_workers = 1;
  TemporalNeutronProcessor processor_1(config_);

  auto start_1 = std::chrono::high_resolution_clock::now();
  auto neutrons_1 = processor_1.processHits(medium_hits);
  auto end_1 = std::chrono::high_resolution_clock::now();
  auto time_1 =
      std::chrono::duration_cast<std::chrono::microseconds>(end_1 - start_1);

  // Test 4 workers
  config_.temporal.num_workers = 4;
  TemporalNeutronProcessor processor_4(config_);

  auto start_4 = std::chrono::high_resolution_clock::now();
  auto neutrons_4 = processor_4.processHits(medium_hits);
  auto end_4 = std::chrono::high_resolution_clock::now();
  auto time_4 =
      std::chrono::duration_cast<std::chrono::microseconds>(end_4 - start_4);

  std::cout << "1 worker: " << time_1.count() << "μs ("
            << time_1.count() / 1000.0 << "ms), " << neutrons_1.size()
            << " neutrons" << std::endl;
  std::cout << "4 workers: " << time_4.count() << "μs ("
            << time_4.count() / 1000.0 << "ms), " << neutrons_4.size()
            << " neutrons" << std::endl;

  if (time_1.count() > 0 && time_4.count() > 0) {
    double speedup = (double)time_1.count() / time_4.count();
    std::cout << "Speedup: " << speedup << "x" << std::endl;

    EXPECT_EQ(neutrons_1.size(), neutrons_4.size())
        << "Same dataset should produce same neutron count";

    // Record timing data for analysis (no failures, just information)
    if (time_1.count() > 1000) {  // If > 1ms
      std::cout << "Processing time is measurable - analyzing speedup..."
                << std::endl;

      if (speedup < 0.8) {
        std::cout << "INFO: 4 workers slower than 1 worker - overhead dominates"
                  << std::endl;
      } else if (speedup < 1.1) {
        std::cout
            << "INFO: Modest or no speedup - typical for memory-bound workloads"
            << std::endl;
      } else {
        std::cout << "GOOD: Significant speedup detected" << std::endl;
      }
    } else {
      std::cout << "Processing too fast to measure speedup reliably"
                << std::endl;
    }
  }
}