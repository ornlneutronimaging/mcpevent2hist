/**
 * @file user_config.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Class to store user-defined configuration for clustering algorithms.
 * @version 0.1
 * @date 2023-09-18
 *
 * @copyright Copyright (c) 2023
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#include "user_config.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <sstream>

/**
 * @brief Construct a new UserConfig object with default values.
 */
UserConfig::UserConfig()
    : m_abs_radius(5.0),
      m_abs_min_cluster_size(1),
      m_abs_spider_time_range(75),
      m_tof_binning(),
      m_super_resolution(1.0) {}

/**
 * @brief Construct a new UserConfig object with user-defined values
 */
UserConfig::UserConfig(double abs_radius,
                       unsigned long int abs_min_cluster_size,
                       unsigned long int abs_spider_time_range)
    : m_abs_radius(abs_radius),
      m_abs_min_cluster_size(abs_min_cluster_size),
      m_abs_spider_time_range(abs_spider_time_range),
      m_tof_binning(),
      m_super_resolution(1.0) {}

/**
 * @brief Helper function to convert a user configuration to a string for
 * console output.
 *
 * @return std::string User configuration as a string
 */
std::string UserConfig::toString() const {
  std::stringstream ss;
  ss << "ABS: radius=" << m_abs_radius
     << ", min_cluster_size=" << m_abs_min_cluster_size
     << ", spider_time_range=" << m_abs_spider_time_range;

  // Add TOF binning information
  if (m_tof_binning.isUniform()) {
    ss << ", TOF bins=" << m_tof_binning.num_bins.value_or(0)
       << ", TOF max=" << (m_tof_binning.tof_max.value_or(0.0) * 1000)
       << " ms";  // Convert to milliseconds
  } else if (m_tof_binning.isCustom()) {
    ss << ", Custom TOF binning with " << m_tof_binning.custom_edges.size() - 1
       << " bins";
  } else {
    ss << ", TOF binning not set";
  }

  ss << ", Super Resolution=" << m_super_resolution;

  return ss.str();
}

/**
 * @brief Parse the user-defined configuration file and return a UserConfig
 * object.
 *
 * @param[in] filepath
 * @return UserConfig User-defined configuration
 */
UserConfig parseUserDefinedConfigurationFile(const std::string& filepath) {
  // Check if the file exists
  if (!std::filesystem::exists(filepath)) {
    spdlog::error("The user-defined configuration file {} does not exist.",
                  filepath);
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
      user_config.setABSSpiderTimeRange(value);
    } else if (name == "tof_bins") {
      int value;
      ss >> value;
    } else if (name == "tof_max") {
      double value;
      ss >> value;
    } else {
      spdlog::warn(
          "Unknown parameter {} in the user-defined configuration file.", name);
    }
  }

  // Close the file
  user_defined_params_file.close();

  // Print the user-defined parameters
  spdlog::info("User-defined parameters: {}", user_config.toString());

  return user_config;
}