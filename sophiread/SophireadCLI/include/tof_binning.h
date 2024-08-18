/**
 * @file tof_binning.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief TOF binning
 * @version 0.1
 * @date 2024-08-16
 * 
 * @copyright Copyright (c) 2024
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <vector>
#include <optional>

struct TOFBinning {
    std::optional<int> num_bins;
    std::optional<double> tof_max;
    std::vector<double> custom_edges;

    // Default constructor
    TOFBinning() : num_bins(1500), tof_max(16.7e-3) {}

    bool isUniform() const {
        return num_bins.has_value() && tof_max.has_value() && custom_edges.empty();
    }

    bool isCustom() const {
        return !custom_edges.empty();
    }

    std::vector<double> getBinEdges() const {
        if (isCustom()) {
            return custom_edges;
        }
        
        int bins = num_bins.value_or(1500);
        double max = tof_max.value_or(16.7e-3);
        std::vector<double> edges(bins + 1);
        for (int i = 0; i <= bins; ++i) {
            edges[i] = max * i / bins;
        }
        return edges;
    }
};