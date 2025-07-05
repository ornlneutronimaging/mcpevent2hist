// TDCSophiread Centroid Peak Fitting Algorithm
// TOT-weighted centroid calculation for sub-pixel neutron position
// determination

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <vector>

#include "tdc_clustering.h"
#include "tdc_clustering_config.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"

namespace tdcsophiread {

/**
 * @brief Centroid peak fitting algorithm implementation
 *
 * Calculates TOT-weighted centroids of hit clusters to determine sub-pixel
 * neutron positions. This is the simpler and faster peak fitting method
 * compared to FastGaussian, suitable for most neutron imaging applications.
 *
 * Algorithm characteristics:
 * - TOT-weighted coordinate calculation for sub-pixel precision
 * - Configurable super-resolution scaling factor
 * - Optional TOT threshold filtering
 * - Fast calculation suitable for real-time processing
 * - VENUS-optimized defaults (8x super-resolution)
 */
class CentroidPeakFitting : public IPeakFitting {
 public:
  /**
   * @brief Constructor with configuration
   * @param config Centroid algorithm parameters
   */
  explicit CentroidPeakFitting(const CentroidConfig& config);

  /**
   * @brief Extract neutron events from clustered hits
   * @param hits Vector of hits with cluster_id assignments
   * @return Vector of neutron events with sub-pixel coordinates
   */
  std::vector<TDCNeutron> extractNeutrons(
      const std::vector<TDCHit>& hits) override;

  /**
   * @brief Get peak fitting statistics from last extraction
   * @return Performance and quality metrics
   */
  struct PeakFittingStats {
    size_t total_hits_processed;  ///< Number of hits processed
    size_t total_clusters_found;  ///< Number of unique clusters
    size_t neutrons_extracted;    ///< Number of neutrons successfully extracted
    size_t hits_below_threshold;  ///< Hits filtered by TOT threshold
    size_t single_hit_neutrons;   ///< Neutrons from single-hit clusters
    size_t multi_hit_neutrons;    ///< Neutrons from multi-hit clusters
    double mean_cluster_size;     ///< Average hits per neutron
    double mean_tot_weight;       ///< Average TOT per neutron
    double processing_time_ms;    ///< Wall-clock processing time
  };

  /**
   * @brief Get statistics from last neutron extraction
   * @return Detailed peak fitting statistics
   */
  PeakFittingStats getStatistics() const;

  /**
   * @brief Reset algorithm state for new dataset
   */
  void reset();

  /**
   * @brief Update algorithm configuration
   * @param config New centroid parameters
   */
  void updateConfig(const CentroidConfig& config);

  // IPeakFitting pure virtual method implementations

  /**
   * @brief Configure algorithm with general clustering configuration
   * @param config General clustering configuration (extracts centroid settings)
   */
  void configure(const ClusteringConfig& config) override;

  /**
   * @brief Get algorithm name
   * @return String identifier for this algorithm
   */
  std::string getName() const override;

  /**
   * @brief Get super-resolution factor applied
   * @return Coordinate scaling factor (e.g., 8 for 8x super-resolution)
   */
  double getSuperResolutionFactor() const override;

  /**
   * @brief Get number of hits processed in last extractNeutrons() call
   * @return Hit count from last peak fitting operation
   */
  size_t getLastHitCount() const;

 private:
  // Configuration parameters
  CentroidConfig config_;

  // Output statistics
  mutable PeakFittingStats stats_;

  // Performance tracking
  mutable std::chrono::high_resolution_clock::time_point start_time_;

  /**
   * @brief Calculate TOT-weighted centroid for a cluster
   * @param cluster_hits All hits belonging to the same cluster
   * @return Neutron event with sub-pixel coordinates
   */
  TDCNeutron calculateCentroid(const std::vector<TDCHit>& cluster_hits) const;

  /**
   * @brief Apply TOT threshold filtering to hits
   * @param hits Input hits to filter
   * @return Filtered hits above threshold
   */
  std::vector<TDCHit> applyTOTFilter(const std::vector<TDCHit>& hits) const;

  /**
   * @brief Calculate TOT-weighted centroid for a cluster using iterators
   * @param begin Iterator to first hit in cluster
   * @param end Iterator past last hit in cluster
   * @return Neutron event with sub-pixel coordinates
   */
  template <typename Iterator>
  TDCNeutron calculateCentroidFromRange(Iterator begin, Iterator end) const;

  /**
   * @brief Update peak fitting statistics
   * @param total_hits Number of input hits
   * @param neutrons Vector of extracted neutrons
   */
  void updateStatistics(size_t total_hits,
                        const std::vector<TDCNeutron>& neutrons);
};

// Template implementation must be in header
template <typename Iterator>
TDCNeutron CentroidPeakFitting::calculateCentroidFromRange(Iterator begin,
                                                           Iterator end) const {
  size_t cluster_size = std::distance(begin, end);

  if (cluster_size == 0) {
    return TDCNeutron();  // Return default neutron
  }

  // Single hit case - no centroid calculation needed
  if (cluster_size == 1) {
    const auto& hit = *begin;
    return TDCNeutron(static_cast<double>(hit.x),  // Native pixel coordinates
                      static_cast<double>(hit.y),  // Native pixel coordinates
                      hit.tof, hit.tot, 1, hit.chip_id);
  }

  // Multi-hit case - calculate TOT-weighted centroid
  double weighted_x = 0.0;
  double weighted_y = 0.0;
  double total_weight = 0.0;
  uint32_t combined_tot = 0;
  uint32_t representative_tof = 0;
  uint8_t chip_id = begin->chip_id;

  if (config_.weighted_by_tot) {
    // TOT-weighted centroid calculation
    for (auto it = begin; it != end; ++it) {
      double weight = static_cast<double>(it->tot);
      weighted_x += static_cast<double>(it->x) * weight;
      weighted_y += static_cast<double>(it->y) * weight;
      total_weight += weight;
      combined_tot += it->tot;
    }

    if (total_weight > 0.0) {
      weighted_x /= total_weight;
      weighted_y /= total_weight;
    }
  } else {
    // Simple arithmetic mean (unweighted)
    for (auto it = begin; it != end; ++it) {
      weighted_x += static_cast<double>(it->x);
      weighted_y += static_cast<double>(it->y);
      combined_tot += it->tot;
    }

    weighted_x /= static_cast<double>(cluster_size);
    weighted_y /= static_cast<double>(cluster_size);
  }

  // Use the TOF from the hit with highest TOT as representative
  auto max_tot_hit = std::max_element(
      begin, end,
      [](const TDCHit& a, const TDCHit& b) { return a.tot < b.tot; });
  representative_tof = max_tot_hit->tof;

  // Return coordinates in native pixel space with sub-pixel precision
  return TDCNeutron(
      weighted_x, weighted_y, representative_tof,
      static_cast<uint16_t>(std::min(
          combined_tot, static_cast<uint32_t>(65535))),  // Clamp to uint16_t
      static_cast<uint16_t>(cluster_size), chip_id);
}

}  // namespace tdcsophiread