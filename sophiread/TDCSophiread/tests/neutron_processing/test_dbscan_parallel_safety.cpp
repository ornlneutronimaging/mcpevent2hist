// TDCSophiread DBSCAN Parallel Safety Tests
// Verify thread safety and memory efficiency for TBB parallel processing

#include <gtest/gtest.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <atomic>
#include <random>
#include <vector>

#include "neutron_processing/dbscan_clustering.h"
#include "neutron_processing/hit_clustering.h"
#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

class DBSCANParallelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create clustering algorithm with default config
    config_.algorithm = "dbscan";
    config_.dbscan.epsilon = 5.0;
    config_.dbscan.min_points = 3;
    config_.dbscan.neutron_correlation_window = 75.0;

    clusterer_ = std::make_unique<DBSCANHitClustering>(config_);
  }

  // Helper to create synthetic hit data
  std::vector<TDCHit> createSyntheticHits(size_t num_hits, unsigned seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<> pos_dist(0, 512);
    std::uniform_int_distribution<> time_dist(0, 10000);

    std::vector<TDCHit> hits;
    hits.reserve(num_hits);

    for (size_t i = 0; i < num_hits; ++i) {
      hits.emplace_back(time_dist(gen),                        // tof
                        static_cast<uint16_t>(pos_dist(gen)),  // x
                        static_cast<uint16_t>(pos_dist(gen)),  // y
                        0,                                     // timestamp
                        100,                                   // tot
                        0                                      // chip_id
      );
    }

    return hits;
  }

  HitClusteringConfig config_;
  std::unique_ptr<DBSCANHitClustering> clusterer_;
};

// Test 1: Verify stateless operation with parallel processing
TEST_F(DBSCANParallelTest, StatelessParallelOperation) {
  // Create a large dataset
  const size_t total_hits = 10000;
  auto hits = createSyntheticHits(total_hits, 42);

  // Divide into batches as temporal processor would
  const size_t batch_size = 1000;
  const size_t num_batches = (total_hits + batch_size - 1) / batch_size;

  // Process batches in parallel using TBB
  std::vector<std::vector<int>> all_labels(num_batches);
  std::vector<size_t> cluster_counts(num_batches);
  std::atomic<bool> had_error(false);

  tbb::parallel_for(tbb::blocked_range<size_t>(0, num_batches),
                    [&](const tbb::blocked_range<size_t>& range) {
                      // Each thread gets its own state - critical for thread
                      // safety
                      auto state = clusterer_->createState();

                      for (size_t batch_idx = range.begin();
                           batch_idx != range.end(); ++batch_idx) {
                        size_t start = batch_idx * batch_size;
                        size_t end = std::min(start + batch_size, total_hits);
                        size_t batch_hits = end - start;

                        // Prepare labels for this batch
                        all_labels[batch_idx].resize(batch_hits, -1);

                        try {
                          // Process batch
                          cluster_counts[batch_idx] = clusterer_->cluster(
                              hits.begin() + start, hits.begin() + end, *state,
                              all_labels[batch_idx]);

                          // Reset state for next batch (as temporal processor
                          // does)
                          state->reset();
                        } catch (...) {
                          had_error = true;
                        }
                      }
                    });

  // Verify no errors occurred
  EXPECT_FALSE(had_error);

  // Verify all batches were processed
  for (size_t i = 0; i < num_batches; ++i) {
    EXPECT_FALSE(all_labels[i].empty());
  }

  // Verify deterministic results by running same batch sequentially
  auto seq_state = clusterer_->createState();
  std::vector<int> seq_labels(batch_size, -1);
  size_t seq_clusters = clusterer_->cluster(
      hits.begin(), hits.begin() + batch_size, *seq_state, seq_labels);

  EXPECT_EQ(cluster_counts[0], seq_clusters);
  EXPECT_EQ(all_labels[0], seq_labels);
}

// Test 2: Verify thread safety with concurrent state usage
TEST_F(DBSCANParallelTest, ConcurrentStateIndependence) {
  // Create identical hit patterns for each thread
  std::vector<TDCHit> hits;

  // Create a known cluster pattern
  for (int i = 0; i < 5; ++i) {
    for (int j = 0; j < 5; ++j) {
      hits.emplace_back(1000, 100 + i, 100 + j, 0, 100, 0);
    }
  }

  // Run multiple threads with same data but different states
  const size_t num_threads = 8;
  std::vector<size_t> results(num_threads);
  std::vector<std::vector<int>> thread_labels(num_threads);

  tbb::parallel_for(size_t(0), num_threads, [&](size_t thread_id) {
    // Each thread must have its own state
    auto state = clusterer_->createState();
    thread_labels[thread_id].resize(hits.size(), -1);

    results[thread_id] = clusterer_->cluster(hits.begin(), hits.end(), *state,
                                             thread_labels[thread_id]);
  });

  // All threads should get identical results
  for (size_t i = 1; i < num_threads; ++i) {
    EXPECT_EQ(results[0], results[i])
        << "Thread " << i << " got different cluster count";
    EXPECT_EQ(thread_labels[0], thread_labels[i])
        << "Thread " << i << " got different labels";
  }
}

// Test 3: Memory efficiency - verify zero-copy with iterators
TEST_F(DBSCANParallelTest, ZeroCopyIteratorProcessing) {
  // Create hits with known clusters
  std::vector<TDCHit> hits;

  // Add some noise hits at the beginning
  for (int i = 0; i < 1000; ++i) {
    hits.emplace_back(i, i * 10 % 512, i * 7 % 512, 0, 100, 0);
  }

  // Add dense clusters in the range we'll process
  for (int cluster = 0; cluster < 5; ++cluster) {
    int base_x = 100 + cluster * 50;
    int base_y = 100 + cluster * 50;
    int base_time = 1000 + cluster * 10;

    // Create 10 hits per cluster (well above minPts=3)
    for (int i = 0; i < 10; ++i) {
      hits.emplace_back(base_time + i / 5,  // time
                        base_x + (i % 3),   // x within epsilon=5
                        base_y + (i / 3),   // y within epsilon=5
                        0, 100, 0);
    }
  }

  // Add more noise at the end
  for (int i = 0; i < 1000; ++i) {
    hits.emplace_back(2000 + i, (i * 13) % 512, (i * 17) % 512, 0, 100, 0);
  }

  // Get baseline memory usage
  auto state = clusterer_->createState();

  // Store pointers to verify no copying
  const void* original_data = hits.data();
  const TDCHit* first_hit_ptr = &hits[0];

  // Process subset using iterators (covers our clusters)
  const size_t start_offset = 1000;
  const size_t end_offset = 1050;  // 50 hits = 5 clusters
  std::vector<int> labels(end_offset - start_offset, -1);

  // Process with iterators
  size_t clusters = clusterer_->cluster(
      hits.begin() + start_offset, hits.begin() + end_offset, *state, labels);

  // Verify original data wasn't moved or copied
  EXPECT_EQ(original_data, hits.data())
      << "Hit vector was reallocated (indicates unwanted copying)";
  EXPECT_EQ(first_hit_ptr, &hits[0]) << "Hit data was moved in memory";

  // Verify we processed the expected range
  EXPECT_EQ(labels.size(), end_offset - start_offset);
  EXPECT_EQ(5, clusters) << "Should find 5 clusters in the processed range";
}

// Test 4: Memory efficiency - verify state doesn't grow unbounded
TEST_F(DBSCANParallelTest, StateMemoryBounded) {
  auto state = clusterer_->createState();
  auto* dbscan_state = dynamic_cast<DBSCANClusteringState*>(state.get());

  // Process multiple batches of increasing size
  for (size_t batch_size : {100, 1000, 10000}) {
    auto hits = createSyntheticHits(batch_size, batch_size);
    std::vector<int> labels(batch_size, -1);

    // Process batch
    clusterer_->cluster(hits.begin(), hits.end(), *state, labels);

    // Check internal state sizes
    EXPECT_EQ(dbscan_state->point_types.size(), batch_size)
        << "point_types vector not sized correctly";
    EXPECT_EQ(dbscan_state->visited.size(), batch_size)
        << "visited vector not sized correctly";

    // Verify spatial index is bounded (64x64 grid)
    const size_t expected_cells = 64 * 64;
    EXPECT_EQ(dbscan_state->spatial_index.cells.size(), expected_cells)
        << "Spatial index has unexpected size";

    // Reset should clear temporary data
    state->reset();
    EXPECT_TRUE(dbscan_state->point_types.empty());
    EXPECT_TRUE(dbscan_state->visited.empty());
  }
}

// Test 5: Race condition test - aggressive parallel clustering
TEST_F(DBSCANParallelTest, NoRaceConditions) {
  // Create challenging dataset with overlapping clusters
  std::vector<TDCHit> hits;

  // Create 10 dense regions
  for (int region = 0; region < 10; ++region) {
    int base_x = region * 50;
    int base_y = region * 50;
    int base_time = region * 10;

    for (int i = 0; i < 10; ++i) {
      hits.emplace_back(base_time + i / 5, base_x + i % 3, base_y + i / 3, 0,
                        100, 0);
    }
  }

  // Run parallel clustering many times to catch race conditions
  const size_t num_iterations = 100;
  std::vector<std::vector<int>> iteration_results(num_iterations);

  tbb::parallel_for(size_t(0), num_iterations, [&](size_t iter) {
    // Each iteration gets fresh state
    auto state = clusterer_->createState();
    iteration_results[iter].resize(hits.size(), -1);

    clusterer_->cluster(hits.begin(), hits.end(), *state,
                        iteration_results[iter]);
  });

  // All iterations should produce identical results
  for (size_t i = 1; i < num_iterations; ++i) {
    EXPECT_EQ(iteration_results[0], iteration_results[i])
        << "Iteration " << i << " produced different results (race condition?)";
  }
}

// Test 6: Verify const-correctness for thread safety
TEST_F(DBSCANParallelTest, ConstCorrectness) {
  // The clustering algorithm should be const (stateless)
  const DBSCANHitClustering* const_clusterer = clusterer_.get();

  auto hits = createSyntheticHits(100, 99);
  auto state = const_clusterer->createState();
  std::vector<int> labels(hits.size(), -1);

  // Should be able to cluster with const clusterer
  size_t clusters =
      const_clusterer->cluster(hits.begin(), hits.end(), *state, labels);

  EXPECT_GE(clusters, 0);

  // Verify configuration is immutable
  auto config1 = const_clusterer->getConfig();

  // Process again
  state->reset();
  const_clusterer->cluster(hits.begin(), hits.end(), *state, labels);

  auto config2 = const_clusterer->getConfig();
  EXPECT_EQ(config1.dbscan.epsilon, config2.dbscan.epsilon);
  EXPECT_EQ(config1.dbscan.min_points, config2.dbscan.min_points);
}

}  // namespace tdcsophiread