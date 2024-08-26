/**
 * @file params.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Class to store user-defined configuration for clustering algorithms.
 * @version 0.1
 * @date 2023-09-18
 *
 * @copyright Copyright (c) 2023
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#pragma once

#include <string>

#include "iconfig.h"
#include "tof_binning.h"

class UserConfig : public IConfig {
 public:
  UserConfig();
  UserConfig(double abs_radius, unsigned long int abs_min_cluster_size,
             unsigned long int abs_spider_time_range);

  double getABSRadius() const override { return m_abs_radius; }
  void setABSRadius(double abs_radius) { m_abs_radius = abs_radius; }

  unsigned long int getABSMinClusterSize() const override {
    return m_abs_min_cluster_size;
  }
  void setABSMinClusterSize(unsigned long int abs_min_cluster_size) {
    m_abs_min_cluster_size = abs_min_cluster_size;
  }

  unsigned long int getABSSpiderTimeRange() const override {
    return m_abs_spider_time_range;
  }
  void setABSSpiderTimeRange(unsigned long int abs_spider_time_range) {
    m_abs_spider_time_range = abs_spider_time_range;
  }

  std::vector<double> getTOFBinEdges() const override {
    return m_tof_binning.getBinEdges();
  }
  void setTOFBinning(const TOFBinning& tof_binning) {
    m_tof_binning = tof_binning;
  }
  void setCustomTOFBinEdges(const std::vector<double>& edges) {
    m_tof_binning.custom_edges = edges;
  }

  // no super resolution for old config format
  double getSuperResolution() const override { return m_super_resolution; }
  void setSuperResolution(double super_resolution) {
    m_super_resolution = super_resolution;
  }

  std::string toString() const override;

 private:
  double m_abs_radius = 5.0;
  unsigned long int m_abs_min_cluster_size = 1;
  unsigned long int m_abs_spider_time_range = 75;
  TOFBinning m_tof_binning;
  double m_super_resolution = 1.0;
};

UserConfig parseUserDefinedConfigurationFile(const std::string& filepath);
