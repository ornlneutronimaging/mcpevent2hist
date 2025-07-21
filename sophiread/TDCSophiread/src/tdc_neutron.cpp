// TDCSophiread Neutron Event Data Structure Implementation
// Implementation of neutron utilities for clustering and analysis

#include "tdc_neutron.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace tdcsophiread {

// NeutronUtils static method implementations

NeutronStatistics NeutronUtils::calculateStatistics(
    const std::vector<TDCNeutron>& neutrons) {
  NeutronStatistics stats;

  if (neutrons.empty()) {
    return stats;
  }

  stats.total_neutrons = neutrons.size();

  // Initialize ranges with first neutron
  stats.tof_min = stats.tof_max = neutrons[0].tof;
  stats.x_min = stats.x_max = neutrons[0].x;
  stats.y_min = stats.y_max = neutrons[0].y;

  double total_cluster_size = 0.0;
  double total_tot = 0.0;

  for (const auto& neutron : neutrons) {
    // Count single vs multi-hit neutrons
    if (neutron.n_hits == 1) {
      stats.single_hit_neutrons++;
    } else {
      stats.multi_hit_neutrons++;
    }

    // Accumulate for means
    total_cluster_size += neutron.n_hits;
    total_tot += neutron.tot;

    // Update ranges
    stats.tof_min = std::min(stats.tof_min, neutron.tof);
    stats.tof_max = std::max(stats.tof_max, neutron.tof);
    stats.x_min = std::min(stats.x_min, neutron.x);
    stats.x_max = std::max(stats.x_max, neutron.x);
    stats.y_min = std::min(stats.y_min, neutron.y);
    stats.y_max = std::max(stats.y_max, neutron.y);

    // Count by chip (ensure chip_id is valid)
    if (neutron.chip_id < stats.chip_counts.size()) {
      stats.chip_counts[neutron.chip_id]++;
    }
  }

  stats.mean_cluster_size = total_cluster_size / stats.total_neutrons;
  stats.mean_tot = total_tot / stats.total_neutrons;

  return stats;
}

std::vector<TDCNeutron> NeutronUtils::filterByROI(
    const std::vector<TDCNeutron>& neutrons, double x_min, double x_max,
    double y_min, double y_max) {
  std::vector<TDCNeutron> filtered;
  filtered.reserve(neutrons.size());

  for (const auto& neutron : neutrons) {
    if (neutron.x >= x_min && neutron.x <= x_max && neutron.y >= y_min &&
        neutron.y <= y_max) {
      filtered.push_back(neutron);
    }
  }

  return filtered;
}

std::vector<TDCNeutron> NeutronUtils::filterByTOF(
    const std::vector<TDCNeutron>& neutrons, uint32_t tof_min,
    uint32_t tof_max) {
  std::vector<TDCNeutron> filtered;
  filtered.reserve(neutrons.size());

  for (const auto& neutron : neutrons) {
    if (neutron.tof >= tof_min && neutron.tof <= tof_max) {
      filtered.push_back(neutron);
    }
  }

  return filtered;
}

std::vector<TDCNeutron> NeutronUtils::filterByClusterSize(
    const std::vector<TDCNeutron>& neutrons, uint16_t min_hits,
    uint16_t max_hits) {
  std::vector<TDCNeutron> filtered;
  filtered.reserve(neutrons.size());

  for (const auto& neutron : neutrons) {
    if (neutron.n_hits >= min_hits && neutron.n_hits <= max_hits) {
      filtered.push_back(neutron);
    }
  }

  return filtered;
}

std::vector<TDCNeutron> NeutronUtils::filterByChip(
    const std::vector<TDCNeutron>& neutrons, uint8_t chip_id) {
  std::vector<TDCNeutron> filtered;
  filtered.reserve(neutrons.size());

  for (const auto& neutron : neutrons) {
    if (neutron.chip_id == chip_id) {
      filtered.push_back(neutron);
    }
  }

  return filtered;
}

std::vector<std::vector<size_t>> NeutronUtils::create2DHistogram(
    const std::vector<TDCNeutron>& neutrons, size_t bins_x, size_t bins_y,
    const std::pair<double, double>& x_range,
    const std::pair<double, double>& y_range) {
  if (neutrons.empty()) {
    return std::vector<std::vector<size_t>>(bins_x,
                                            std::vector<size_t>(bins_y, 0));
  }

  // Determine ranges
  double x_min, x_max, y_min, y_max;

  if (x_range.first == x_range.second) {
    // Auto-detect X range
    auto x_minmax = std::minmax_element(
        neutrons.begin(), neutrons.end(),
        [](const TDCNeutron& a, const TDCNeutron& b) { return a.x < b.x; });
    x_min = x_minmax.first->x;
    x_max = x_minmax.second->x;
  } else {
    x_min = x_range.first;
    x_max = x_range.second;
  }

  if (y_range.first == y_range.second) {
    // Auto-detect Y range
    auto y_minmax = std::minmax_element(
        neutrons.begin(), neutrons.end(),
        [](const TDCNeutron& a, const TDCNeutron& b) { return a.y < b.y; });
    y_min = y_minmax.first->y;
    y_max = y_minmax.second->y;
  } else {
    y_min = y_range.first;
    y_max = y_range.second;
  }

  // Create histogram
  std::vector<std::vector<size_t>> histogram(bins_x,
                                             std::vector<size_t>(bins_y, 0));

  double x_bin_width = (x_max - x_min) / bins_x;
  double y_bin_width = (y_max - y_min) / bins_y;

  for (const auto& neutron : neutrons) {
    // Calculate bin indices
    size_t x_bin = static_cast<size_t>((neutron.x - x_min) / x_bin_width);
    size_t y_bin = static_cast<size_t>((neutron.y - y_min) / y_bin_width);

    // Handle edge case where neutron is exactly at max value
    if (x_bin >= bins_x) x_bin = bins_x - 1;
    if (y_bin >= bins_y) y_bin = bins_y - 1;

    histogram[x_bin][y_bin]++;
  }

  return histogram;
}

std::pair<std::vector<double>, std::vector<size_t>>
NeutronUtils::createTOFSpectrum(
    const std::vector<TDCNeutron>& neutrons, size_t bins,
    const std::pair<uint32_t, uint32_t>& tof_range) {
  std::vector<double> bin_centers(bins);
  std::vector<size_t> counts(bins, 0);

  if (neutrons.empty()) {
    return {bin_centers, counts};
  }

  // Determine TOF range
  uint32_t tof_min, tof_max;

  if (tof_range.first == tof_range.second) {
    // Auto-detect range
    auto minmax = std::minmax_element(
        neutrons.begin(), neutrons.end(),
        [](const TDCNeutron& a, const TDCNeutron& b) { return a.tof < b.tof; });
    tof_min = minmax.first->tof;
    tof_max = minmax.second->tof;
  } else {
    tof_min = tof_range.first;
    tof_max = tof_range.second;
  }

  double bin_width = static_cast<double>(tof_max - tof_min) / bins;

  // Calculate bin centers
  for (size_t i = 0; i < bins; ++i) {
    bin_centers[i] = tof_min + (i + 0.5) * bin_width;
  }

  // Fill histogram
  for (const auto& neutron : neutrons) {
    if (neutron.tof >= tof_min && neutron.tof <= tof_max) {
      size_t bin = static_cast<size_t>((neutron.tof - tof_min) / bin_width);
      if (bin >= bins) bin = bins - 1;  // Handle edge case
      counts[bin]++;
    }
  }

  return {bin_centers, counts};
}

std::pair<std::vector<uint16_t>, std::vector<size_t>>
NeutronUtils::createClusterSizeDistribution(
    const std::vector<TDCNeutron>& neutrons) {
  if (neutrons.empty()) {
    return {{}, {}};
  }

  // Find unique cluster sizes
  std::vector<uint16_t> cluster_sizes;
  for (const auto& neutron : neutrons) {
    if (std::find(cluster_sizes.begin(), cluster_sizes.end(), neutron.n_hits) ==
        cluster_sizes.end()) {
      cluster_sizes.push_back(neutron.n_hits);
    }
  }

  // Sort cluster sizes
  std::sort(cluster_sizes.begin(), cluster_sizes.end());

  // Count occurrences
  std::vector<size_t> counts(cluster_sizes.size(), 0);
  for (const auto& neutron : neutrons) {
    auto it =
        std::find(cluster_sizes.begin(), cluster_sizes.end(), neutron.n_hits);
    if (it != cluster_sizes.end()) {
      size_t index = std::distance(cluster_sizes.begin(), it);
      counts[index]++;
    }
  }

  return {cluster_sizes, counts};
}

std::vector<TDCHit> NeutronUtils::convertToHits(
    const std::vector<TDCNeutron>& neutrons) {
  std::vector<TDCHit> hits;
  hits.reserve(neutrons.size());

  for (const auto& neutron : neutrons) {
    TDCHit hit;
    hit.x = static_cast<uint16_t>(std::round(neutron.x));
    hit.y = static_cast<uint16_t>(std::round(neutron.y));
    hit.tof = neutron.tof;
    hit.tot = neutron.tot;
    hit.chip_id = neutron.chip_id;
    hit.timestamp = neutron.tof;  // Use TOF as timestamp for converted hits
    hit.cluster_id = -1;          // Not clustered anymore

    hits.push_back(hit);
  }

  return hits;
}

void NeutronUtils::sortByTOF(std::vector<TDCNeutron>& neutrons,
                             bool ascending) {
  if (ascending) {
    std::sort(
        neutrons.begin(), neutrons.end(),
        [](const TDCNeutron& a, const TDCNeutron& b) { return a.tof < b.tof; });
  } else {
    std::sort(
        neutrons.begin(), neutrons.end(),
        [](const TDCNeutron& a, const TDCNeutron& b) { return a.tof > b.tof; });
  }
}

void NeutronUtils::sortByClusterSize(std::vector<TDCNeutron>& neutrons,
                                     bool ascending) {
  if (ascending) {
    std::sort(neutrons.begin(), neutrons.end(),
              [](const TDCNeutron& a, const TDCNeutron& b) {
                return a.n_hits < b.n_hits;
              });
  } else {
    std::sort(neutrons.begin(), neutrons.end(),
              [](const TDCNeutron& a, const TDCNeutron& b) {
                return a.n_hits > b.n_hits;
              });
  }
}

bool NeutronUtils::validateData(const std::vector<TDCNeutron>& neutrons) {
  for (const auto& neutron : neutrons) {
    // Check for valid cluster size
    if (neutron.n_hits == 0) {
      return false;
    }

    // Check for reasonable coordinate values (no negative coordinates)
    if (neutron.x < 0.0 || neutron.y < 0.0) {
      return false;
    }

    // Check for valid chip ID (0-3 for 2x2 layout)
    if (neutron.chip_id > 3) {
      return false;
    }
  }

  return true;
}

}  // namespace tdcsophiread