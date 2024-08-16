/**
 * @file json_config_parser.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Class to store user-defined configuration (JSON) for clustering algorithms.
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
#include "json_config_parser.h"
#include <fstream>
#include <spdlog/spdlog.h>

JSONConfigParser JSONConfigParser::fromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open configuration file: " + filepath);
    }

    nlohmann::json config;
    try {
        file >> config;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Error parsing JSON file: " + std::string(e.what()));
    }

    return JSONConfigParser(config);
}

JSONConfigParser::JSONConfigParser(const nlohmann::json& config) : m_config(config) {}

double JSONConfigParser::getABSRadius() const {
    return m_config["abs"]["radius"].get<double>();
}

unsigned long int JSONConfigParser::getABSMinClusterSize() const {
    return m_config["abs"]["min_cluster_size"].get<unsigned long int>();
}

unsigned long int JSONConfigParser::getABSSpiderTimeRange() const {
    return m_config["abs"]["spider_time_range"].get<unsigned long int>();
}

std::vector<double> JSONConfigParser::getTOFBinEdges() const {
    const auto& tof = m_config["tof_imaging"];
    if (tof.contains("bin_edges")) {
        return tof["bin_edges"].get<std::vector<double>>();
    } else if (tof.contains("uniform_bins")) {
        const auto& uniform = tof["uniform_bins"];
        double start = uniform["start"].get<double>();
        double end = uniform["end"].get<double>();
        int num_bins = uniform["num_bins"].get<int>();
        std::vector<double> edges(num_bins + 1);
        for (int i = 0; i <= num_bins; ++i) {
            edges[i] = start + (end - start) * i / num_bins;
        }
        return edges;
    }
    throw std::runtime_error("TOF bin edges not specified in configuration");
}

std::string JSONConfigParser::toString() const {
    return m_config.dump(2);
}