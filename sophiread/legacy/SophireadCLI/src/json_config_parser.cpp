/**
 * @file json_config_parser.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Class to store user-defined configuration (JSON) for clustering
 * algorithms.
 * @version 0.1
 * @date 2024-08-16
 *
 * @copyright Copyright (c) 2024
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#include "json_config_parser.h"

#include <spdlog/spdlog.h>

#include <fstream>

JSONConfigParser JSONConfigParser::createDefault() {
  nlohmann::json default_config = {
      {"abs",
       {{"radius", DEFAULT_ABS_RADIUS},
        {"min_cluster_size", DEFAULT_ABS_MIN_CLUSTER_SIZE},
        {"spider_time_range", DEFAULT_ABS_SPIDER_TIME_RANGE}}},
      {"tof_imaging",
       {{"uniform_bins",
         {{"num_bins", DEFAULT_TOF_BINS}, {"end", DEFAULT_TOF_MAX}}},
        {"super_resolution", DEFAULT_SUPER_RESOLUTION}}}};

  return JSONConfigParser(default_config);
}

/**
 * @brief Build JSONConfigParser from a given file
 * @param filepath Path to the JSON configuration file
 * @return JSONConfigParser
 */
JSONConfigParser JSONConfigParser::fromFile(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open configuration file: " + filepath);
  }

  nlohmann::json config;
  try {
    file >> config;
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error("Error parsing JSON file: " +
                             std::string(e.what()));
  }

  return JSONConfigParser(config);
}

/**
 * @brief Construct a new JSONConfigParser::JSONConfigParser object
 * @param config JSON configuration object
 */
JSONConfigParser::JSONConfigParser(const nlohmann::json& config)
    : m_config(config) {
  parseTOFBinning();
}

/**
 * @brief Get the ABS Radius
 * @return double
 */
double JSONConfigParser::getABSRadius() const {
  return m_config.value("/abs/radius"_json_pointer, DEFAULT_ABS_RADIUS);
}

/**
 * @brief Get the ABS Min Cluster Size
 * @return unsigned long int
 */
unsigned long int JSONConfigParser::getABSMinClusterSize() const {
  return m_config.value("/abs/min_cluster_size"_json_pointer,
                        DEFAULT_ABS_MIN_CLUSTER_SIZE);
}

/**
 * @brief Get the ABS Spider Time Range
 * @return unsigned long int
 */
unsigned long int JSONConfigParser::getABSSpiderTimeRange() const {
  return m_config.value("/abs/spider_time_range"_json_pointer,
                        DEFAULT_ABS_SPIDER_TIME_RANGE);
}

/**
 * @brief Get the TOF Bin Edges
 * @return std::vector<double>
 */
std::vector<double> JSONConfigParser::getTOFBinEdges() const {
  return m_tof_binning.getBinEdges();
}

double JSONConfigParser::getSuperResolution() const {
  return m_config.value("/tof_imaging/super_resolution"_json_pointer,
                        DEFAULT_SUPER_RESOLUTION);
}

/**
 * @brief Parse the TOF binning configuration
 */
void JSONConfigParser::parseTOFBinning() {
  if (m_config.contains("/tof_imaging/bin_edges"_json_pointer)) {
    m_tof_binning.custom_edges = m_config["/tof_imaging/bin_edges"_json_pointer]
                                     .get<std::vector<double>>();
  } else if (m_config.contains("/tof_imaging/uniform_bins"_json_pointer)) {
    const auto& uniform = m_config["/tof_imaging/uniform_bins"_json_pointer];
    m_tof_binning.num_bins = uniform.value("num_bins", DEFAULT_TOF_BINS);
    m_tof_binning.tof_max = uniform.value("end", DEFAULT_TOF_MAX);
  } else {
    // Use default values
    m_tof_binning.num_bins = DEFAULT_TOF_BINS;
    m_tof_binning.tof_max = DEFAULT_TOF_MAX;
  }
}

/**
 * @brief Get a string representation of the configuration
 * @return std::string
 */
std::string JSONConfigParser::toString() const {
  std::stringstream ss;
  ss << "ABS: radius=" << getABSRadius()
     << ", min_cluster_size=" << getABSMinClusterSize()
     << ", spider_time_range=" << getABSSpiderTimeRange();

  if (m_tof_binning.isCustom()) {
    ss << ", Custom TOF binning with " << m_tof_binning.custom_edges.size() - 1
       << " bins";
  } else {
    ss << ", TOF bins=" << m_tof_binning.num_bins.value_or(DEFAULT_TOF_BINS)
       << ", TOF max="
       << (m_tof_binning.tof_max.value_or(DEFAULT_TOF_MAX) * 1000) << " ms";
  }

  ss << ", Super Resolution=" << getSuperResolution();

  return ss.str();
}
