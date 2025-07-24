// TDCSophiread ABS Bucket
// Data structure for Age-Based Spatial clustering buckets

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "tdc_hit.h"

namespace tdcsophiread {

/**
 * @brief Bucket structure for ABS clustering algorithm
 *
 * Represents a cluster candidate with spatial bounds and temporal window.
 * Used by the ABS clustering algorithm to group hits that are spatially
 * and temporally correlated.
 */
struct ABSBucket {
  // Hit indices (LOCAL to iterator range)
  std::vector<size_t> hit_indices;

  // Spatial bounds (detector coordinates)
  int x_min, x_max, y_min, y_max;

  // Temporal window (TOF of first hit)
  uint32_t start_timestamp;

  // Cluster assignment (-1 if not yet assigned)
  int32_t cluster_label;

  // Active state
  bool is_active;

  /**
   * @brief Default constructor
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
    if (hit.x < x_min - radius || hit.x > x_max + radius) return false;
    if (hit.y < y_min - radius || hit.y > y_max + radius) return false;
    return true;
  }

  /**
   * @brief Check if hit fits temporally within bucket
   * @param hit Hit to test
   * @param window_ns Correlation window in nanoseconds
   * @return True if hit is within temporal constraints
   */
  bool fitsTemporally(const TDCHit& hit, double window_ns) const {
    uint32_t window_tdc = static_cast<uint32_t>(window_ns / 25.0);
    uint32_t tof_diff = (hit.tof >= start_timestamp)
                            ? (hit.tof - start_timestamp)
                            : (start_timestamp - hit.tof);
    return tof_diff <= window_tdc;
  }

  /**
   * @brief Check if bucket is too old to receive new hits
   * @param reference_tof Current TOF for age calculation
   * @param window_ns Correlation window in nanoseconds
   * @return True if bucket is aged out
   */
  bool isAged(uint32_t reference_tof, double window_ns) const {
    uint32_t window_tdc = static_cast<uint32_t>(window_ns / 25.0);
    uint32_t age = (reference_tof >= start_timestamp)
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
    x_min = x_max = 0;
    y_min = y_max = 0;
    start_timestamp = 0;
    cluster_label = -1;
    is_active = false;
  }
};

}  // namespace tdcsophiread