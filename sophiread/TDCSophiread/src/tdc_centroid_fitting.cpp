// TDCSophiread Centroid Peak Fitting Implementation
// TOT-weighted centroid calculation for sub-pixel neutron position
// determination

#include "tdc_centroid_fitting.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <numeric>

namespace tdcsophiread {

CentroidPeakFitting::CentroidPeakFitting(const CentroidConfig& config)
    : config_(config) {
  reset();
}

std::vector<TDCNeutron> CentroidPeakFitting::extractNeutrons(
    const std::vector<TDCHit>& hits) {
  start_time_ = std::chrono::high_resolution_clock::now();

  std::vector<TDCNeutron> neutrons;

  if (hits.empty()) {
    updateStatistics(0, neutrons);
    return neutrons;
  }

  // Apply TOT filtering if enabled
  std::vector<TDCHit> filtered_hits = applyTOTFilter(hits);
  stats_.hits_below_threshold = hits.size() - filtered_hits.size();

  // Group hits by cluster ID
  auto cluster_groups = groupHitsByCluster(filtered_hits);
  stats_.total_clusters_found = cluster_groups.size();

  // Process each cluster to extract neutrons
  neutrons.reserve(cluster_groups.size());

  for (const auto& [cluster_id, cluster_hits] : cluster_groups) {
    if (cluster_id == -1) {
      continue;  // Skip unclustered hits
    }

    if (cluster_hits.empty()) {
      continue;  // Skip empty clusters
    }

    // Calculate centroid for this cluster
    TDCNeutron neutron = calculateCentroid(cluster_hits);
    neutrons.push_back(neutron);

    // Update cluster size statistics
    if (neutron.n_hits == 1) {
      stats_.single_hit_neutrons++;
    } else {
      stats_.multi_hit_neutrons++;
    }
  }

  updateStatistics(hits.size(), neutrons);
  return neutrons;
}

TDCNeutron CentroidPeakFitting::calculateCentroid(
    const std::vector<TDCHit>& cluster_hits) const {
  if (cluster_hits.empty()) {
    return TDCNeutron();  // Return default neutron
  }

  // Single hit case - no centroid calculation needed
  if (cluster_hits.size() == 1) {
    const auto& hit = cluster_hits[0];
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
  uint8_t chip_id = cluster_hits[0].chip_id;

  if (config_.weighted_by_tot) {
    // TOT-weighted centroid calculation
    for (const auto& hit : cluster_hits) {
      double weight = static_cast<double>(hit.tot);
      weighted_x += static_cast<double>(hit.x) * weight;
      weighted_y += static_cast<double>(hit.y) * weight;
      total_weight += weight;
      combined_tot += hit.tot;
    }

    if (total_weight > 0.0) {
      weighted_x /= total_weight;
      weighted_y /= total_weight;
    }
  } else {
    // Simple arithmetic mean (unweighted)
    for (const auto& hit : cluster_hits) {
      weighted_x += static_cast<double>(hit.x);
      weighted_y += static_cast<double>(hit.y);
      combined_tot += hit.tot;
    }

    weighted_x /= static_cast<double>(cluster_hits.size());
    weighted_y /= static_cast<double>(cluster_hits.size());
  }

  // Use the TOF from the hit with highest TOT as representative
  auto max_tot_hit = std::max_element(
      cluster_hits.begin(), cluster_hits.end(),
      [](const TDCHit& a, const TDCHit& b) { return a.tot < b.tot; });
  representative_tof = max_tot_hit->tof;

  // Return coordinates in native pixel space with sub-pixel precision
  return TDCNeutron(
      weighted_x, weighted_y, representative_tof,
      static_cast<uint16_t>(std::min(
          combined_tot, static_cast<uint32_t>(65535))),  // Clamp to uint16_t
      static_cast<uint16_t>(cluster_hits.size()), chip_id);
}

std::vector<TDCHit> CentroidPeakFitting::applyTOTFilter(
    const std::vector<TDCHit>& hits) const {
  if (config_.min_tot_threshold <= 0.0) {
    return hits;  // No filtering needed
  }

  std::vector<TDCHit> filtered_hits;
  filtered_hits.reserve(hits.size());

  for (const auto& hit : hits) {
    if (static_cast<double>(hit.tot) >= config_.min_tot_threshold) {
      filtered_hits.push_back(hit);
    }
  }

  return filtered_hits;
}

std::map<int, std::vector<TDCHit>> CentroidPeakFitting::groupHitsByCluster(
    const std::vector<TDCHit>& hits) const {
  std::map<int, std::vector<TDCHit>> cluster_groups;

  for (const auto& hit : hits) {
    cluster_groups[hit.cluster_id].push_back(hit);
  }

  return cluster_groups;
}

void CentroidPeakFitting::updateStatistics(
    size_t total_hits, const std::vector<TDCNeutron>& neutrons) {
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time_);

  stats_.total_hits_processed = total_hits;
  stats_.neutrons_extracted = neutrons.size();
  stats_.processing_time_ms = duration.count() / 1000.0;

  // Calculate mean cluster size and TOT
  if (!neutrons.empty()) {
    double total_cluster_size = 0.0;
    double total_tot = 0.0;

    for (const auto& neutron : neutrons) {
      total_cluster_size += neutron.n_hits;
      total_tot += neutron.tot;
    }

    stats_.mean_cluster_size = total_cluster_size / neutrons.size();
    stats_.mean_tot_weight = total_tot / neutrons.size();
  } else {
    stats_.mean_cluster_size = 0.0;
    stats_.mean_tot_weight = 0.0;
  }
}

CentroidPeakFitting::PeakFittingStats CentroidPeakFitting::getStatistics()
    const {
  return stats_;
}

void CentroidPeakFitting::reset() { stats_ = PeakFittingStats{}; }

void CentroidPeakFitting::updateConfig(const CentroidConfig& config) {
  config_ = config;
}

void CentroidPeakFitting::configure(const ClusteringConfig& config) {
  config_ = config.centroid;
}

std::string CentroidPeakFitting::getName() const { return "centroid"; }

double CentroidPeakFitting::getSuperResolutionFactor() const {
  return config_.super_resolution_factor;
}

size_t CentroidPeakFitting::getLastHitCount() const {
  return stats_.total_hits_processed;
}

}  // namespace tdcsophiread