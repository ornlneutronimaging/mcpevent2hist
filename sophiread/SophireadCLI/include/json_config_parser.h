/**
 * @file json_config_parser.h
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
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "iconfig.h"
#include "tof_binning.h"

class JSONConfigParser : public IConfig {
 public:
  static JSONConfigParser createDefault();
  static JSONConfigParser fromFile(const std::string& filepath);

  double getABSRadius() const override;
  unsigned long int getABSMinClusterSize() const override;
  unsigned long int getABSSpiderTimeRange() const override;
  std::vector<double> getTOFBinEdges() const override;
  double getSuperResolution() const override;

  std::string toString() const override;

 private:
  JSONConfigParser(const nlohmann::json& config);
  nlohmann::json m_config;
  TOFBinning m_tof_binning;

  void parseTOFBinning();

  // Default values
  static constexpr double DEFAULT_ABS_RADIUS = 5.0;
  static constexpr unsigned long int DEFAULT_ABS_MIN_CLUSTER_SIZE = 1;
  static constexpr unsigned long int DEFAULT_ABS_SPIDER_TIME_RANGE = 75;
  static constexpr int DEFAULT_TOF_BINS = 1500;
  static constexpr double DEFAULT_TOF_MAX = 16.7e-3;  // 16.7 milliseconds
  static constexpr double DEFAULT_SUPER_RESOLUTION = 1.0;
};
