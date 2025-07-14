// TDCSophiread ABS Clustering
// Primary implementation of Age-Based Spatial clustering (stateless)

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "neutron_processing/abs_bucket.h"
#include "neutron_processing/clustering_state.h"
#include "neutron_processing/hit_clustering.h"
#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

/**
 * @brief Primary ABS clustering implementation (stateless)
 *
 * Implements Age-Based Spatial clustering as a stateless algorithm.
 * All working state is passed via the ABSClusteringState parameter,
 * enabling safe concurrent execution by multiple threads.
 *
 * This is the production implementation designed to meet the
 * >120M hits/sec throughput requirement through parallel processing.
 */
class ABSClustering : public IHitClustering {
 private:
  // Configuration (immutable after construction)
  HitClusteringConfig config_;

  // Constants from optimized implementation
  static constexpr size_t SPATIAL_GRID_SIZE = 32;
  static constexpr uint8_t SPATIAL_BIN_SIZE = 8;

  /**
   * @brief Find bucket that can accommodate the hit
   * @param hit Hit to test
   * @param state Clustering state containing buckets
   * @return Index of compatible bucket, or -1 if none found
   */
  int findCompatibleBucket(const TDCHit& hit,
                           const ABSClusteringState& state) const;

  /**
   * @brief Get or create a bucket for the hit
   * @param hit_index LOCAL index of hit in the iterator range
   * @param hit Hit data
   * @param state Clustering state to modify
   * @return Index of bucket (new or reused)
   */
  size_t getOrCreateBucket(size_t hit_index, const TDCHit& hit,
                           ABSClusteringState& state) const;

  /**
   * @brief Free a bucket for reuse
   * @param bucket_index Index of bucket to free
   * @param state Clustering state to modify
   */
  void freeBucket(size_t bucket_index, ABSClusteringState& state) const;

  /**
   * @brief Scan for and close aged buckets
   * @param reference_tof Current TOF for age calculation
   * @param state Clustering state to modify
   * @param[out] cluster_labels Output cluster labels
   */
  void scanAndCloseAgedBuckets(uint32_t reference_tof,
                               ABSClusteringState& state,
                               std::vector<int>& cluster_labels) const;

  /**
   * @brief Close a bucket and assign cluster IDs
   * @param bucket_index Index of bucket to close
   * @param state Clustering state to modify
   * @param[out] cluster_labels Output cluster labels
   */
  void closeBucket(size_t bucket_index, ABSClusteringState& state,
                   std::vector<int>& cluster_labels) const;

  /**
   * @brief Get spatial bin coordinates for a hit
   * @param hit Hit to get bin coordinates for
   * @return Pair of (bin_x, bin_y) coordinates
   */
  std::pair<size_t, size_t> getSpatialBin(const TDCHit& hit) const;

  /**
   * @brief Add bucket to spatial index
   * @param bucket_idx Index of bucket to add
   * @param hit Hit that defines the bucket's initial position
   * @param state Clustering state to modify
   */
  void addBucketToSpatialIndex(size_t bucket_idx, const TDCHit& hit,
                               ABSClusteringState& state) const;

  /**
   * @brief Remove bucket from spatial index
   * @param bucket_idx Index of bucket to remove
   * @param state Clustering state to modify
   */
  void removeBucketFromSpatialIndex(size_t bucket_idx,
                                    ABSClusteringState& state) const;

 public:
  /**
   * @brief Default constructor with ABS defaults
   */
  ABSClustering();

  /**
   * @brief Constructor with specific configuration
   */
  explicit ABSClustering(const HitClusteringConfig& config);

  // IHitClustering interface implementation
  void configure(const HitClusteringConfig& config) override;
  const HitClusteringConfig& getConfig() const override { return config_; }

  std::unique_ptr<IClusteringState> createState() const override;

  size_t cluster(std::vector<TDCHit>::const_iterator begin,
                 std::vector<TDCHit>::const_iterator end,
                 IClusteringState& state,
                 std::vector<int>& cluster_labels) const override;

  std::string getName() const override { return "abs"; }

  ClusteringStatistics getStatistics(const IClusteringState& state,
                                     size_t num_hits) const override;
};

}  // namespace tdcsophiread