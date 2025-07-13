// TDCSophiread Simple ABS Clustering Implementation
// Basic Age-Based Spatial clustering adapted to new iterator interface

#pragma once

#include <algorithm>
#include <vector>

#include "neutron_processing/hit_clustering.h"
#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

/**
 * @brief Bucket structure for ABS clustering (reusable)
 *
 * Adapted from legacy ABSBucket with same spatial and temporal logic.
 * CRITICAL: This is a REUSABLE container that gets reset and returned
 * to the pool after bucket closure to prevent memory explosion.
 */
struct SimpleABSBucket {
  std::vector<size_t> hit_indices;  ///< LOCAL indices of hits in this bucket
                                    ///< (0-based within iterator range)
  int x_min, x_max;                 ///< Spatial bounding box (X range)
  int y_min, y_max;                 ///< Spatial bounding box (Y range)
  uint32_t start_timestamp;         ///< Bucket creation time (TOF) - FIRST hit
  int32_t cluster_label;            ///< Assigned cluster ID (-1 = unassigned)
  bool is_active;  ///< Whether bucket is actively collecting hits

  /**
   * @brief Default constructor - creates inactive bucket
   */
  SimpleABSBucket()
      : x_min(0),
        x_max(0),
        y_min(0),
        y_max(0),
        start_timestamp(0),
        cluster_label(-1),
        is_active(false) {
    hit_indices.reserve(16);  // Reserve space for typical neutron cluster
  }

  /**
   * @brief Initialize bucket with first hit
   * @param hit_index LOCAL index of initial hit (within iterator range)
   * @param hit Hit data for spatial bounds
   */
  void initialize(size_t hit_index, const TDCHit& hit) {
    hit_indices.clear();
    hit_indices.push_back(hit_index);
    x_min = x_max = hit.x;
    y_min = y_max = hit.y;
    start_timestamp = hit.tof;  // Use FIRST hit timing
    cluster_label = -1;         // Unassigned until bucket is closed
    is_active = true;
  }

  /**
   * @brief Add hit to existing bucket
   * @param hit_index LOCAL index of hit to add (within iterator range)
   * @param hit Hit data for spatial bounds update
   */
  void addHit(size_t hit_index, const TDCHit& hit) {
    hit_indices.push_back(hit_index);
    x_min = std::min(x_min, static_cast<int>(hit.x));
    x_max = std::max(x_max, static_cast<int>(hit.x));
    y_min = std::min(y_min, static_cast<int>(hit.y));
    y_max = std::max(y_max, static_cast<int>(hit.y));
  }

  /**
   * @brief Check if hit fits spatially within bucket (box distance)
   * @param hit Hit to test
   * @param radius Maximum distance from cluster boundary
   * @return True if hit is within spatial constraints
   */
  bool fitsSpatially(const TDCHit& hit, double radius) const {
    const int r = static_cast<int>(radius);
    return (hit.x >= x_min - r) && (hit.x <= x_max + r) &&
           (hit.y >= y_min - r) && (hit.y <= y_max + r);
  }

  /**
   * @brief Check if hit fits temporally within bucket
   * @param hit Hit to test
   * @param correlation_window Maximum time difference in nanoseconds
   * @return True if hit is within temporal constraints
   */
  bool fitsTemporally(const TDCHit& hit, double correlation_window) const {
    const uint32_t window_tdc =
        static_cast<uint32_t>(correlation_window / 25.0);
    const uint32_t time_diff = (hit.tof > start_timestamp)
                                   ? (hit.tof - start_timestamp)
                                   : (start_timestamp - hit.tof);
    return time_diff <= window_tdc;
  }

  /**
   * @brief Check if bucket is aged based on reference time
   * @param reference_tof Current reference time (TOF)
   * @param correlation_window Aging threshold in nanoseconds
   * @return True if bucket should be closed due to aging
   */
  bool isAged(uint32_t reference_tof, double correlation_window) const {
    const uint32_t window_tdc =
        static_cast<uint32_t>(correlation_window / 25.0);
    const uint32_t age = (reference_tof > start_timestamp)
                             ? (reference_tof - start_timestamp)
                             : (start_timestamp - reference_tof);
    return age > window_tdc;
  }

  /**
   * @brief Close bucket and assign cluster ID if sufficient hits
   * @param min_cluster_size Minimum hits required for valid cluster
   * @param next_cluster_id Next available cluster ID
   * @return True if cluster was formed, false if hits remain unclustered
   */
  bool closeBucket(uint16_t min_cluster_size, int32_t next_cluster_id) {
    is_active = false;
    if (hit_indices.size() >= min_cluster_size) {
      cluster_label = next_cluster_id;
      return true;  // Valid cluster formed
    }
    cluster_label = -1;  // Insufficient hits - remain unclustered
    return false;        // No cluster formed
  }

  /**
   * @brief Reset bucket to initial state for reuse
   */
  void reset() {
    hit_indices.clear();
    cluster_label = -1;
    is_active = false;
  }
};

/**
 * @brief Simple ABS clustering implementation with bucket pool
 *
 * Implements the bucket pool system from legacy code, adapted to store
 * cluster labels separately instead of modifying hits directly.
 * Uses LOCAL indices within iterator range for thread safety.
 */
class SimpleABSClustering : public IHitClustering {
 private:
  // Configuration
  HitClusteringConfig config_;

  // Bucket pool management (adapted from legacy)
  std::vector<SimpleABSBucket> bucket_pool_;  ///< Pool of reusable buckets
  std::vector<size_t>
      active_bucket_indices_;  ///< Indices of currently active buckets
  std::vector<size_t>
      free_bucket_indices_;  ///< Indices of free buckets for reuse

  // Processing state (LOCAL to iterator range)
  std::vector<int>
      cluster_labels_;       ///< Cluster labels for each hit in iterator range
  size_t last_hit_count_;    ///< Number of hits processed in last run
  int32_t next_cluster_id_;  ///< Next available cluster ID
  size_t hits_processed_;    ///< Hit counter for scan timing

  // Performance optimization: pre-allocated vectors to avoid hot-path
  // allocations
  mutable std::vector<size_t>
      buckets_to_close_;  ///< Reusable vector for aged bucket collection
  mutable std::vector<size_t>
      remaining_buckets_;  ///< Reusable vector for final bucket closure

  // Spatial indexing optimization: simple spatial hash for faster bucket lookup
  struct SpatialBin {
    std::vector<size_t> bucket_indices;  ///< Bucket indices in this spatial bin
    void addBucket(size_t bucket_idx) { bucket_indices.push_back(bucket_idx); }
    void removeBucket(size_t bucket_idx) {
      auto it =
          std::find(bucket_indices.begin(), bucket_indices.end(), bucket_idx);
      if (it != bucket_indices.end()) {
        std::swap(*it, bucket_indices.back());
        bucket_indices.pop_back();
      }
    }
    void clear() { bucket_indices.clear(); }
  };

  static constexpr size_t SPATIAL_GRID_SIZE = 32;  ///< 32x32 spatial grid
  static constexpr size_t SPATIAL_BIN_SIZE =
      8;  ///< 8x8 pixel bins for 256x256 detector
  std::array<std::array<SpatialBin, SPATIAL_GRID_SIZE>, SPATIAL_GRID_SIZE>
      spatial_bins_;

  // Initial bucket pool size
  static constexpr size_t INITIAL_BUCKET_POOL_SIZE = 100;

  /**
   * @brief Find bucket that can accommodate the hit
   * @param hit Hit to test
   * @return Index of compatible bucket, or -1 if none found
   */
  int findCompatibleBucket(const TDCHit& hit) const;

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
   */
  void addBucketToSpatialIndex(size_t bucket_idx, const TDCHit& hit);

  /**
   * @brief Remove bucket from spatial index
   * @param bucket_idx Index of bucket to remove
   * @param hit Hit that defines the bucket's position
   */
  void removeBucketFromSpatialIndex(size_t bucket_idx, const TDCHit& hit);

  /**
   * @brief Get or create bucket for hit
   * @param hit_index LOCAL index of hit in iterator range
   * @param hit Hit data for bucket initialization
   * @return Index of bucket (from pool)
   */
  size_t getOrCreateBucket(size_t hit_index, const TDCHit& hit);

  /**
   * @brief Free bucket back to pool for reuse
   * @param bucket_index Index of bucket to free
   */
  void freeBucket(size_t bucket_index);

  /**
   * @brief Scan for aged buckets and close them
   * @param reference_tof Current reference time for aging check
   */
  void scanAndCloseAgedBuckets(uint32_t reference_tof);

  /**
   * @brief Close bucket and assign cluster IDs to LOCAL cluster_labels_ vector
   * @param bucket_index Index of bucket to close
   */
  void closeBucket(size_t bucket_index);

 public:
  /**
   * @brief Default constructor with ABS defaults
   */
  SimpleABSClustering();

  /**
   * @brief Constructor with specific configuration
   */
  explicit SimpleABSClustering(const HitClusteringConfig& config);

  // IHitClustering interface implementation
  void configure(const HitClusteringConfig& config) override;
  const HitClusteringConfig& getConfig() const override { return config_; }

  size_t cluster(std::vector<TDCHit>::const_iterator begin,
                 std::vector<TDCHit>::const_iterator end) override;

  void reset() override;
  const std::vector<int>& getClusterLabels() const override {
    return cluster_labels_;
  }
  std::string getName() const override;
  size_t getLastHitCount() const override { return last_hit_count_; }
  ClusteringStatistics getStatistics() const override;
};

}  // namespace tdcsophiread