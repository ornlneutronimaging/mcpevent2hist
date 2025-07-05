// TDCSophiread ABS (Adaptive Box Search) Clustering Algorithm
// High-performance spatial-temporal clustering for neutron event detection

#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <vector>

#include "tdc_clustering.h"
#include "tdc_clustering_config.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"

namespace tdcsophiread {

/**
 * @brief Bucket for dynamic ABS clustering algorithm
 *
 * Dynamic structure using hit indices instead of copying hit data.
 * Maintains spatial bounding box and temporal information.
 */
struct ABSBucket {
  std::vector<size_t> hit_indices;  ///< Indices of hits in this bucket
  int x_min, x_max;                 ///< Spatial bounding box (X range)
  int y_min, y_max;                 ///< Spatial bounding box (Y range)
  uint32_t start_timestamp;         ///< Bucket creation time (TOF)
  int32_t cluster_label;            ///< Assigned cluster ID (-1 = unassigned)
  bool is_active;  ///< Whether bucket is actively collecting hits

  // IMPORTANT: This is a REUSABLE container. After closing:
  // 1. Hits get their cluster_id assigned (or left as -1)
  // 2. Bucket is reset() and returned to free pool
  // 3. Same bucket object is reused for future hits
  // This prevents creating millions of bucket objects

  /**
   * @brief Default constructor - creates inactive bucket
   */
  ABSBucket()
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
   * @param hit_index Index of initial hit
   * @param hit Initial hit for spatial bounds
   */
  void initialize(size_t hit_index, const TDCHit& hit) {
    hit_indices.clear();
    hit_indices.push_back(hit_index);
    x_min = x_max = hit.x;
    y_min = y_max = hit.y;
    start_timestamp = hit.tof;
    cluster_label = -1;  // Unassigned until bucket is closed
    is_active = true;
  }

  /**
   * @brief Add hit to existing bucket
   * @param hit_index Index of hit to add
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
   * @brief Check if hit fits spatially within cluster
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
   * @brief Get number of hits in bucket
   * @return Hit count
   */
  size_t getHitCount() const { return hit_indices.size(); }

  /**
   * @brief Reset bucket to initial state
   */
  void reset() {
    hit_indices.clear();
    cluster_label = -1;
    is_active = false;
  }
};

/**
 * @brief ABS (Adaptive Box Search) clustering algorithm implementation
 *
 * Physics-correct spatial-temporal clustering using dynamic bucket system.
 * Designed for neutron detection with proper gamma noise filtering.
 *
 * CRITICAL DESIGN UNDERSTANDING:
 * - Buckets are TEMPORARY workspaces for collecting correlated hits
 * - Active bucket count is determined by physics (no artificial limits)
 * - After aging: bucket assigns cluster_id to hits, then is FREED for reuse
 * - Bucket pool grows as needed, freed buckets are reused
 *
 * Algorithm flow:
 * 1. New hit arrives → find compatible active bucket or get/create one
 * 2. Every scan_interval hits → scan all ACTIVE buckets for aging
 * 3. Aged bucket → if hits >= min_cluster_size: assign cluster_id
 *                  else: leave hits as cluster_id = -1 (gamma noise)
 * 4. Close bucket → mark as free for reuse (bucket object recycled)
 *
 * Memory management:
 * - Initial pool capacity (e.g., 1000) for efficiency
 * - Pool grows dynamically if all buckets are active
 * - Freed buckets reused before growing pool
 * - No maximum limit - physics determines active bucket count
 *
 * Performance characteristics:
 * - O(active_buckets) scan complexity, NOT O(total_clusters_ever)
 * - Active buckets typically ~100-1000 (depends on beam intensity)
 * - Bucket reuse minimizes memory allocations
 */
class ABSClustering : public IClusteringAlgorithm {
 public:
  /**
   * @brief Constructor with configuration
   * @param config ABS algorithm parameters
   */
  explicit ABSClustering(const ABSConfig& config);

  // Initial bucket pool size (will grow as needed)
  static constexpr size_t INITIAL_BUCKET_POOL_SIZE = 1000;

  /**
   * @brief Apply clustering to hit data
   * @param hits Vector of hits to cluster (modified to add cluster labels)
   * @return Number of clusters found
   */
  size_t fit(std::vector<TDCHit>& hits) override;

  /**
   * @brief Get cluster labels assigned during fit()
   * @return Vector of cluster labels (one per hit)
   */
  const std::vector<int>& getClusterLabels() const override;

  /**
   * @brief Get clustering statistics from last fit() call
   * @return Performance and quality metrics
   */
  struct ClusteringStats {
    size_t total_hits;          ///< Number of hits processed
    size_t total_clusters;      ///< Number of valid clusters created
    size_t total_buckets;       ///< Number of buckets created
    size_t gamma_hits;          ///< Hits rejected as gamma noise
    size_t aged_bucket_scans;   ///< Number of aging scans performed
    double mean_cluster_size;   ///< Average hits per valid cluster
    double processing_time_ms;  ///< Wall-clock processing time
  };

  /**
   * @brief Get statistics from last clustering operation
   * @return Detailed clustering statistics
   */
  ClusteringStats getStatistics() const;

  /**
   * @brief Reset algorithm state for new dataset
   */
  void reset() override;

  /**
   * @brief Update algorithm configuration
   * @param config New ABS parameters
   */
  void updateConfig(const ABSConfig& config);

  // IClusteringAlgorithm pure virtual method implementations

  /**
   * @brief Configure algorithm with general clustering configuration
   * @param config General clustering configuration (extracts ABS settings)
   */
  void configure(const ClusteringConfig& config) override;

  /**
   * @brief Get algorithm name
   * @return String identifier for this algorithm
   */
  std::string getName() const override;

  /**
   * @brief Get number of hits processed in last fit() call
   * @return Hit count from last clustering operation
   */
  size_t getLastHitCount() const override;

 private:
  // Configuration parameters
  ABSConfig config_;

  // Bucket pool management
  std::vector<ABSBucket> bucket_pool_;  ///< Pool of reusable buckets
  std::vector<size_t>
      active_bucket_indices_;  ///< Indices of currently active buckets
  std::vector<size_t>
      free_bucket_indices_;  ///< Indices of free buckets for reuse

  // Hit data reference
  std::vector<TDCHit>* hits_ptr_;  ///< Pointer to current hit data

  // Cluster management
  int32_t next_cluster_label_;  ///< Next available cluster label
  size_t hits_processed_;       ///< Number of hits processed (for scan timing)

  // Output data
  std::vector<int> cluster_labels_;  ///< Cluster assignment for each hit

  // Statistics
  mutable ClusteringStats stats_;

  // Performance tracking
  mutable std::chrono::high_resolution_clock::time_point start_time_;

  /**
   * @brief Find bucket that can accommodate the hit
   * @param hit Hit to test
   * @return Index of compatible bucket, or -1 if none found
   */
  int findCompatibleBucket(const TDCHit& hit) const;

  /**
   * @brief Get or create bucket for hit
   * @param hit_index Index of hit in hits vector
   * @param hit Hit data for bucket initialization
   * @return Index of bucket (from pool)
   *
   * Logic:
   * 1. If free buckets available → reuse one
   * 2. If no free buckets → grow pool and use new bucket
   * Never fails - pool grows as needed by physics
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
   * @brief Close bucket and assign cluster IDs to hits
   * @param bucket_index Index of bucket to close
   */
  void closeBucket(size_t bucket_index);

  /**
   * @brief Update clustering statistics
   */
  void updateStatistics();
};

}  // namespace tdcsophiread