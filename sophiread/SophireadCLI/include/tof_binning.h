/**
 * @file tof_binning.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief TOF binning
 * @version 0.1
 * @date 2024-08-16
 *
 * @copyright Copyright (c) 2024
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#pragma once

#include <optional>
#include <vector>

struct TOFBinning {
  std::optional<int> num_bins;
  std::optional<double> tof_max;
  std::vector<double> custom_edges;

  // Default constructor
  TOFBinning() : num_bins(1500), tof_max(1.0 / 60) {}

  bool isUniform() const {
    return num_bins.has_value() && tof_max.has_value() && custom_edges.empty();
  }

  bool isCustom() const { return !custom_edges.empty(); }

  std::vector<double> getBinEdges() const {
    if (isCustom()) {
      return custom_edges;
    }

    int bins = num_bins.value_or(1500);
    double max = tof_max.value_or(1.0 / 60);
    std::vector<double> edges(bins + 1);
    for (int i = 0; i <= bins; ++i) {
      edges[i] = max * i / bins;
    }
    return edges;
  }
};