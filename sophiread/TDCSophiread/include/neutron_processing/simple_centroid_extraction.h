// TDCSophiread Simple Centroid Extraction Implementation
// Basic centroid calculation adapted to new iterator interface with separate
// cluster labels

#pragma once

#include <vector>

#include "neutron_processing/neutron_config.h"
#include "neutron_processing/neutron_extraction.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"

namespace tdcsophiread {

/**
 * @brief Simple centroid-based neutron extraction implementation
 *
 * Adapted from legacy CentroidPeakFitting but works with separate cluster
 * labels vector instead of modifying hits directly. Uses the same TOT-weighted
 * centroid calculation and representative TOF selection logic.
 */
class SimpleCentroidExtraction : public INeutronExtraction {
 private:
  NeutronExtractionConfig config_;
  ExtractionStatistics last_stats_;

  /**
   * @brief Calculate TOT-weighted centroid for a cluster using iterators
   * @param begin Iterator to first hit in cluster
   * @param end Iterator past last hit in cluster
   * @return Neutron event with sub-pixel coordinates
   *
   * Adapted from legacy template function calculateCentroidFromRange()
   */
  template <typename Iterator>
  TDCNeutron calculateCentroidFromRange(Iterator begin, Iterator end) const;

  /**
   * @brief Apply TOT threshold filtering to cluster
   * @param begin Iterator to first hit in cluster
   * @param end Iterator past last hit in cluster
   * @return Vector of filtered hits above threshold
   */
  template <typename Iterator>
  std::vector<TDCHit> applyTOTFilter(Iterator begin, Iterator end) const;

 public:
  /**
   * @brief Default constructor with centroid defaults
   */
  SimpleCentroidExtraction();

  /**
   * @brief Constructor with specific configuration
   */
  explicit SimpleCentroidExtraction(const NeutronExtractionConfig& config);

  // INeutronExtraction interface implementation
  void configure(const NeutronExtractionConfig& config) override;
  const NeutronExtractionConfig& getConfig() const override { return config_; }

  std::vector<TDCNeutron> extract(
      std::vector<TDCHit>::const_iterator begin,
      std::vector<TDCHit>::const_iterator end,
      const std::vector<int>& cluster_labels) override;

  void reset() override;
  std::string getName() const override;
  double getSuperResolutionFactor() const override;
  ExtractionStatistics getStatistics() const override;
};

// Template implementation must be in header (adapted from legacy)
template <typename Iterator>
TDCNeutron SimpleCentroidExtraction::calculateCentroidFromRange(
    Iterator begin, Iterator end) const {
  size_t cluster_size = std::distance(begin, end);

  if (cluster_size == 0) {
    return TDCNeutron();  // Return default neutron
  }

  // Single hit case - no centroid calculation needed
  if (cluster_size == 1) {
    const auto& hit = *begin;
    return TDCNeutron(
        static_cast<double>(hit.x) *
            config_.super_resolution_factor,  // Apply super-resolution scaling
        static_cast<double>(hit.y) *
            config_.super_resolution_factor,  // Apply super-resolution scaling
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

  // Return coordinates in super-resolution space with sub-pixel precision
  return TDCNeutron(
      weighted_x *
          config_.super_resolution_factor,  // Apply super-resolution scaling
      weighted_y *
          config_.super_resolution_factor,  // Apply super-resolution scaling
      representative_tof,
      static_cast<uint16_t>(std::min(
          combined_tot, static_cast<uint32_t>(65535))),  // Clamp to uint16_t
      static_cast<uint16_t>(cluster_size), chip_id);
}

template <typename Iterator>
std::vector<TDCHit> SimpleCentroidExtraction::applyTOTFilter(
    Iterator begin, Iterator end) const {
  if (config_.min_tot_threshold <= 0.0) {
    // No filtering needed - return all hits
    std::vector<TDCHit> all_hits;
    all_hits.reserve(std::distance(begin, end));
    for (auto it = begin; it != end; ++it) {
      all_hits.push_back(*it);
    }
    return all_hits;
  }

  std::vector<TDCHit> filtered_hits;
  filtered_hits.reserve(std::distance(begin, end));

  for (auto it = begin; it != end; ++it) {
    if (static_cast<double>(it->tot) >= config_.min_tot_threshold) {
      filtered_hits.push_back(*it);
    }
  }

  return filtered_hits;
}

}  // namespace tdcsophiread