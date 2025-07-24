// TDCSophiread Clustering State Infrastructure
// Defines state management for stateless clustering algorithms

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "neutron_processing/abs_bucket.h"  // For ABSBucket

namespace tdcsophiread {

/**
 * @brief Base interface for algorithm-specific clustering state
 *
 * Each clustering algorithm defines its own state implementation
 * containing the working data structures needed during clustering.
 * This enables stateless algorithm implementations that can be
 * safely used concurrently by multiple threads.
 */
class IClusteringState {
 public:
  virtual ~IClusteringState() = default;

  /**
   * @brief Reset state to initial conditions
   *
   * Clears all working data structures while preserving capacity
   * for efficient reuse.
   */
  virtual void reset() = 0;

  /**
   * @brief Get algorithm name this state is for
   */
  virtual std::string getAlgorithmName() const = 0;
};

/**
 * @brief Clustering state for ABS (Age-Based Spatial) algorithm
 *
 * Contains all working data structures needed by the ABS clustering
 * algorithm. This state is owned by the HitBatch and passed to the
 * stateless ABS algorithm for processing.
 */
class ABSClusteringState : public IClusteringState {
 public:
  // Bucket pool for cluster candidates
  std::vector<ABSBucket> bucket_pool;

  // Active bucket management
  std::vector<size_t> active_bucket_indices;
  std::vector<size_t> free_bucket_indices;

  // Spatial indexing for O(1) bucket lookup
  static constexpr size_t SPATIAL_GRID_SIZE = 32;
  struct SpatialBin {
    std::vector<size_t> bucket_indices;
    void clear() { bucket_indices.clear(); }
  };
  std::array<std::array<SpatialBin, SPATIAL_GRID_SIZE>, SPATIAL_GRID_SIZE>
      spatial_bins;

  // Temporary vectors for bulk operations (pre-allocated)
  std::vector<size_t> buckets_to_close;
  std::vector<size_t> remaining_buckets;

  // Processing state
  int next_cluster_id = 0;
  size_t hits_processed = 0;

  /**
   * @brief Constructor with optional pre-allocation
   */
  explicit ABSClusteringState(size_t initial_bucket_capacity = 1000);

  void reset() override;
  std::string getAlgorithmName() const override { return "simple_abs"; }
};

/**
 * @brief Factory for creating algorithm-specific clustering states
 *
 * Creates the appropriate state object based on algorithm name.
 * This factory ensures each batch gets the correct state type
 * for its clustering algorithm.
 */
class ClusteringStateFactory {
 public:
  /**
   * @brief Create state object for the specified algorithm
   * @param algorithm_name Name of clustering algorithm ("simple_abs", "graph",
   * etc.)
   * @return Unique pointer to algorithm-specific state
   * @throws std::invalid_argument if algorithm_name is unknown
   */
  static std::unique_ptr<IClusteringState> create(
      const std::string& algorithm_name);

  /**
   * @brief Check if algorithm is supported
   */
  static bool isSupported(const std::string& algorithm_name);
};

}  // namespace tdcsophiread