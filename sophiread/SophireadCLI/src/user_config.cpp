/**
 * @file user_config.cpp
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

#include "user_config.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <sstream>

/**
 * @brief Helper function to convert a user configuration to a string for console output.
 *
 * @return std::string
 */
std::string UserConfig::toString() const {
  std::stringstream ss;
  ss << "ABS: radius=" << m_abs_radius << ", min_cluster_size=" << m_abs_min_cluster_size
     << ", spider_time_range=" << m_abs_spider_time_range;

  return ss.str();
}

/**
 * @brief Parse the user-defined configuration file and return a UserConfig object.
 *
 * @param[in] filepath
 * @return UserConfig
 */
UserConfig parseUserDefinedConfigurationFile(const std::string& filepath) {
  // Check if the file exists
  if (!std::filesystem::exists(filepath)) {
    spdlog::error("The user-defined configuration file {} does not exist.", filepath);
    exit(1);
  }

  // Create the default UserConfig object
  UserConfig user_config;

  //
  std::ifstream user_defined_params_file(filepath);
  if (!user_defined_params_file.is_open()) {
    spdlog::error("Failed to open {}.", filepath);
    spdlog::warn("Fallback to default user configurations!");
    return user_config;
  }

  std::string line;
  while (std::getline(user_defined_params_file, line)) {
    std::istringstream ss(line);
    std::string name;
    ss >> name;

    // skip comments
    if (name[0] == '#') {
      continue;
    }

    if (name == "abs_radius") {
      double value;
      ss >> value;
      user_config.setABSRadius(value);
    } else if (name == "abs_min_cluster_size") {
      int value;
      ss >> value;
      user_config.setABSMinClusterSize(value);
    } else if (name == "spider_time_range") {
      int value;
      ss >> value;
      user_config.setABSSpidertimeRange(value);
    } else {
      spdlog::warn("Unknown parameter {} in the user-defined configuration file.", name);
    }
  }

  // Close the file
  user_defined_params_file.close();

  // Print the user-defined parameters
  spdlog::info("User-defined parameters: {}", user_config.toString());

  return user_config;
}