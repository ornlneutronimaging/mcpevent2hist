// TDCSophiread Simple Centroid Extraction Implementation
// Basic centroid calculation adapted to new iterator interface with separate
// cluster labels

#include "neutron_processing/simple_centroid_extraction.h"

#include <algorithm>
#include <chrono>
#include <map>

namespace tdcsophiread {

SimpleCentroidExtraction::SimpleCentroidExtraction()
    : config_(), last_stats_() {
  // Initialize with default centroid configuration
  config_.algorithm = "centroid";
  config_.super_resolution_factor = 8.0;
  config_.weighted_by_tot = true;
  config_.min_tot_threshold = 10;
}

SimpleCentroidExtraction::SimpleCentroidExtraction(
    const NeutronExtractionConfig& config)
    : config_(config), last_stats_() {
  config_.validate();
}

void SimpleCentroidExtraction::configure(
    const NeutronExtractionConfig& config) {
  config.validate();
  config_ = config;
  reset();
}

std::vector<TDCNeutron> SimpleCentroidExtraction::extract(
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end,
    const std::vector<int>& cluster_labels) {
  auto start_time = std::chrono::high_resolution_clock::now();

  const size_t num_hits = std::distance(begin, end);
  std::vector<TDCNeutron> neutrons;

  if (num_hits == 0 || cluster_labels.size() != num_hits) {
    last_stats_ = ExtractionStatistics{};
    return neutrons;
  }

  // Group hits by cluster ID (adapted from legacy approach)
  // Create map of cluster_id -> vector of hit indices
  std::map<int, std::vector<size_t>> cluster_map;

  for (size_t i = 0; i < num_hits; ++i) {
    int cluster_id = cluster_labels[i];
    if (cluster_id >= 0) {  // Skip unclustered hits (-1)
      cluster_map[cluster_id].push_back(i);
    }
  }

  // Reserve space for neutrons
  neutrons.reserve(cluster_map.size());

  // Process each cluster
  size_t single_hit_neutrons = 0;
  size_t multi_hit_neutrons = 0;
  size_t total_hits_processed = 0;

  for (const auto& cluster_pair : cluster_map) {
    const std::vector<size_t>& hit_indices = cluster_pair.second;

    if (hit_indices.empty()) continue;

    // Create vector of hits for this cluster
    std::vector<TDCHit> cluster_hits;
    cluster_hits.reserve(hit_indices.size());

    for (size_t hit_idx : hit_indices) {
      auto hit_it = begin + hit_idx;
      cluster_hits.push_back(*hit_it);
    }

    total_hits_processed += cluster_hits.size();

    // Apply TOT filtering if enabled
    std::vector<TDCHit> filtered_hits;
    if (config_.min_tot_threshold > 0.0) {
      for (const auto& hit : cluster_hits) {
        if (static_cast<double>(hit.tot) >= config_.min_tot_threshold) {
          filtered_hits.push_back(hit);
        }
      }
    } else {
      filtered_hits = cluster_hits;
    }

    // Skip clusters with no hits after filtering
    if (filtered_hits.empty()) continue;

    // Calculate centroid for this cluster
    TDCNeutron neutron =
        calculateCentroidFromRange(filtered_hits.begin(), filtered_hits.end());
    neutrons.push_back(neutron);

    // Update statistics
    if (filtered_hits.size() == 1) {
      single_hit_neutrons++;
    } else {
      multi_hit_neutrons++;
    }
  }

  // Update statistics
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
      end_time - start_time);

  last_stats_.total_hits_processed = total_hits_processed;
  last_stats_.total_clusters_processed = cluster_map.size();
  last_stats_.total_neutrons_extracted = neutrons.size();
  last_stats_.rejected_clusters = cluster_map.size() - neutrons.size();
  // Convert nanoseconds to milliseconds with higher precision
  // Use minimum of 0.001ms (1 microsecond) to avoid exactly 0.0
  last_stats_.processing_time_ms =
      std::max(0.001, duration.count() / 1000000.0);
  last_stats_.single_hit_neutrons = single_hit_neutrons;
  last_stats_.multi_hit_neutrons = multi_hit_neutrons;

  if (!neutrons.empty()) {
    double total_cluster_size = 0.0;
    for (const auto& neutron : neutrons) {
      total_cluster_size += neutron.n_hits;
    }
    last_stats_.mean_hits_per_neutron = total_cluster_size / neutrons.size();
    last_stats_.extraction_efficiency =
        static_cast<double>(neutrons.size()) / cluster_map.size();
  } else {
    last_stats_.mean_hits_per_neutron = 0.0;
    last_stats_.extraction_efficiency = 0.0;
  }

  return neutrons;
}

// Template instantiation for the calculateCentroidFromRange function
// (Implementation moved to header file due to template requirements)

void SimpleCentroidExtraction::reset() { last_stats_ = ExtractionStatistics{}; }

std::string SimpleCentroidExtraction::getName() const {
  return "simple_centroid";
}

double SimpleCentroidExtraction::getSuperResolutionFactor() const {
  return config_.super_resolution_factor;
}

ExtractionStatistics SimpleCentroidExtraction::getStatistics() const {
  return last_stats_;
}

}  // namespace tdcsophiread