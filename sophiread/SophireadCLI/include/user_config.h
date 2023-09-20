/**
 * @file params.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Class to store user-defined configuration for clustering algorithms.
 * @version 0.1
 * @date 2023-09-18
 *
 * @copyright Copyright (c) 2023
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

#include <string>

class UserConfig {
 public:
  UserConfig(){};
  UserConfig(const double abs_radius, unsigned long int abs_min_cluster_size, unsigned long int abs_spider_time_range)
      : m_abs_radius(abs_radius),
        m_abs_min_cluster_size(abs_min_cluster_size),
        m_abs_spider_time_range(abs_spider_time_range){};

  double getABSRadius() const { return m_abs_radius; };
  void setABSRadius(const double abs_radius) { m_abs_radius = abs_radius; };

  unsigned long int getABSMinClusterSize() const { return m_abs_min_cluster_size; };
  void setABSMinClusterSize(const unsigned long int abs_min_cluster_size) {
    m_abs_min_cluster_size = abs_min_cluster_size;
  };

  unsigned long int getABSSpidertimeRange() const { return m_abs_spider_time_range; };
  void setABSSpidertimeRange(const unsigned long int abs_spider_time_range) {
    m_abs_spider_time_range = abs_spider_time_range;
  };

  std::string toString() const;

 private:
  // ABS members (see abs.h for details)
  double m_abs_radius = 5.0;
  unsigned long int m_abs_min_cluster_size = 1;
  unsigned long int m_abs_spider_time_range = 75;
};

UserConfig parseUserDefinedConfigurationFile(const std::string& filepath);