/**
 * @file json_config_parser.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Class to store user-defined configuration (JSON) for clustering
 * algorithms.
 * @version 0.1
 * @date 2024-08-16
 *
 * @copyright Copyright (c) 2024
 * SPDX - License - Identifier: GPL - 3.0 +
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
