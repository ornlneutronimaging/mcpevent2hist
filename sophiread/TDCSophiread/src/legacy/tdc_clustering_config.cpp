// TDCSophiread Clustering Configuration Implementation

#include "tdc_clustering_config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace tdcsophiread {

// ==================== ABSConfig ====================

void ABSConfig::validate() const {
  if (radius <= 0.0) {
    throw std::invalid_argument("ABS radius must be positive, got: " +
                                std::to_string(radius));
  }
  if (radius > 50.0) {
    throw std::invalid_argument("ABS radius too large (>50 pixels), got: " +
                                std::to_string(radius));
  }
  if (min_cluster_size == 0) {
    throw std::invalid_argument("ABS min_cluster_size must be positive, got: " +
                                std::to_string(min_cluster_size));
  }
  if (neutron_correlation_window <= 0.0) {
    throw std::invalid_argument(
        "ABS neutron_correlation_window must be positive, got: " +
        std::to_string(neutron_correlation_window));
  }
  if (neutron_correlation_window > 10000.0) {
    throw std::invalid_argument(
        "ABS neutron_correlation_window too large (>10μs), got: " +
        std::to_string(neutron_correlation_window));
  }
  if (scan_interval == 0) {
    throw std::invalid_argument("ABS scan_interval must be positive, got: " +
                                std::to_string(scan_interval));
  }
}

void ABSConfig::fromJson(const nlohmann::json& json) {
  if (json.contains("radius")) {
    radius = json["radius"];
  }
  if (json.contains("min_cluster_size")) {
    min_cluster_size = json["min_cluster_size"];
  }
  // Support both old and new parameter names for backward compatibility
  if (json.contains("neutron_correlation_window")) {
    neutron_correlation_window = json["neutron_correlation_window"];
  } else if (json.contains("time_range_ns")) {
    // Backward compatibility
    neutron_correlation_window = json["time_range_ns"];
  }
  if (json.contains("scan_interval")) {
    scan_interval = json["scan_interval"];
  }
  // Note: max_clusters is deprecated in new implementation
  // Silently ignore if present for backward compatibility
  validate();
}

nlohmann::json ABSConfig::toJson() const {
  return nlohmann::json{
      {"radius", radius},
      {"min_cluster_size", min_cluster_size},
      {"neutron_correlation_window", neutron_correlation_window},
      {"scan_interval", scan_interval}};
}

// ==================== CentroidConfig ====================

void CentroidConfig::validate() const {
  if (super_resolution_factor <= 0.0) {
    throw std::invalid_argument(
        "Centroid super_resolution_factor must be positive, got: " +
        std::to_string(super_resolution_factor));
  }
  if (super_resolution_factor > 100.0) {
    throw std::invalid_argument(
        "Centroid super_resolution_factor too large (>100), got: " +
        std::to_string(super_resolution_factor));
  }
  if (min_tot_threshold < 0.0) {
    throw std::invalid_argument(
        "Centroid min_tot_threshold must be non-negative, got: " +
        std::to_string(min_tot_threshold));
  }
}

void CentroidConfig::fromJson(const nlohmann::json& json) {
  if (json.contains("super_resolution_factor")) {
    super_resolution_factor = json["super_resolution_factor"];
  }
  if (json.contains("weighted_by_tot")) {
    weighted_by_tot = json["weighted_by_tot"];
  }
  if (json.contains("min_tot_threshold")) {
    min_tot_threshold = json["min_tot_threshold"];
  }
  validate();
}

nlohmann::json CentroidConfig::toJson() const {
  return nlohmann::json{{"super_resolution_factor", super_resolution_factor},
                        {"weighted_by_tot", weighted_by_tot},
                        {"min_tot_threshold", min_tot_threshold}};
}

// ==================== FastGaussianConfig ====================

void FastGaussianConfig::validate() const {
  if (super_resolution_factor <= 0.0) {
    throw std::invalid_argument(
        "FastGaussian super_resolution_factor must be positive, got: " +
        std::to_string(super_resolution_factor));
  }
  if (min_cluster_size < 4) {
    throw std::invalid_argument(
        "FastGaussian min_cluster_size must be ≥4, got: " +
        std::to_string(min_cluster_size));
  }
  if (tot_filter_fraction < 0.0 || tot_filter_fraction >= 1.0) {
    throw std::invalid_argument(
        "FastGaussian tot_filter_fraction must be [0,1), got: " +
        std::to_string(tot_filter_fraction));
  }
  if (max_iterations == 0) {
    throw std::invalid_argument(
        "FastGaussian max_iterations must be positive, got: " +
        std::to_string(max_iterations));
  }
  if (convergence_tolerance <= 0.0) {
    throw std::invalid_argument(
        "FastGaussian convergence_tolerance must be positive, got: " +
        std::to_string(convergence_tolerance));
  }
}

void FastGaussianConfig::fromJson(const nlohmann::json& json) {
  if (json.contains("super_resolution_factor")) {
    super_resolution_factor = json["super_resolution_factor"];
  }
  if (json.contains("min_cluster_size")) {
    min_cluster_size = json["min_cluster_size"];
  }
  if (json.contains("tot_filter_fraction")) {
    tot_filter_fraction = json["tot_filter_fraction"];
  }
  if (json.contains("max_iterations")) {
    max_iterations = json["max_iterations"];
  }
  if (json.contains("convergence_tolerance")) {
    convergence_tolerance = json["convergence_tolerance"];
  }
  validate();
}

nlohmann::json FastGaussianConfig::toJson() const {
  return nlohmann::json{{"super_resolution_factor", super_resolution_factor},
                        {"min_cluster_size", min_cluster_size},
                        {"tot_filter_fraction", tot_filter_fraction},
                        {"max_iterations", max_iterations},
                        {"convergence_tolerance", convergence_tolerance}};
}

// ==================== ClusteringConfig ====================

ClusteringConfig ClusteringConfig::venusDefaults() {
  ClusteringConfig config;

  // Algorithm selection optimized for VENUS detector
  config.clustering_algorithm = "abs";
  config.peak_fitting_algorithm = "centroid";

  // ABS parameters optimized for VENUS neutron imaging
  config.abs.radius = 5.0;          // 5-pixel spatial clustering radius
  config.abs.min_cluster_size = 1;  // Include single-hit neutrons
  config.abs.neutron_correlation_window = 75.0;  // 75ns temporal window
  config.abs.scan_interval = 100;                // Scan every 100 hits

  // Centroid parameters for 8x super-resolution
  config.centroid.super_resolution_factor = 8.0;
  config.centroid.weighted_by_tot = true;
  config.centroid.min_tot_threshold = 0.0;

  // FastGaussian parameters (when enabled)
  config.fastgaussian.super_resolution_factor = 8.0;
  config.fastgaussian.min_cluster_size = 8;
  config.fastgaussian.tot_filter_fraction = 0.5;

  config.enable_clustering = true;
  config.enable_performance_logging = false;

  return config;
}

ClusteringConfig ClusteringConfig::fromFile(const std::string& config_path) {
  std::ifstream file(config_path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open clustering configuration file: " +
                             config_path);
  }

  nlohmann::json json_config;
  try {
    file >> json_config;
  } catch (const nlohmann::json::parse_error& e) {
    throw std::runtime_error("JSON parse error in " + config_path + ": " +
                             e.what());
  }

  return fromJson(json_config);
}

ClusteringConfig ClusteringConfig::fromJson(const nlohmann::json& json) {
  ClusteringConfig config = venusDefaults();  // Start with defaults

  if (!json.contains("clustering")) {
    return config;  // Use defaults if no clustering section
  }

  const auto& clustering = json["clustering"];

  // Algorithm selection
  if (clustering.contains("clustering_algorithm")) {
    config.clustering_algorithm = clustering["clustering_algorithm"];
  }
  if (clustering.contains("peak_fitting_algorithm")) {
    config.peak_fitting_algorithm = clustering["peak_fitting_algorithm"];
  }

  // General options
  if (clustering.contains("enable_clustering")) {
    config.enable_clustering = clustering["enable_clustering"];
  }
  if (clustering.contains("enable_performance_logging")) {
    config.enable_performance_logging =
        clustering["enable_performance_logging"];
  }

  // Algorithm-specific configurations
  if (clustering.contains("abs")) {
    config.abs.fromJson(clustering["abs"]);
  }
  if (clustering.contains("centroid")) {
    config.centroid.fromJson(clustering["centroid"]);
  }
  if (clustering.contains("fastgaussian")) {
    config.fastgaussian.fromJson(clustering["fastgaussian"]);
  }

  config.validate();
  return config;
}

void ClusteringConfig::validate() const {
  // Validate algorithm names
  if (!isClusteringAlgorithmSupported(clustering_algorithm)) {
    throw std::invalid_argument("Unsupported clustering algorithm: " +
                                clustering_algorithm);
  }
  if (!isPeakFittingAlgorithmSupported(peak_fitting_algorithm)) {
    throw std::invalid_argument("Unsupported peak fitting algorithm: " +
                                peak_fitting_algorithm);
  }

  // Validate algorithm-specific configurations
  abs.validate();
  centroid.validate();
  fastgaussian.validate();
}

nlohmann::json ClusteringConfig::toJson() const {
  return nlohmann::json{
      {"clustering",
       {{"clustering_algorithm", clustering_algorithm},
        {"peak_fitting_algorithm", peak_fitting_algorithm},
        {"enable_clustering", enable_clustering},
        {"enable_performance_logging", enable_performance_logging},
        {"abs", abs.toJson()},
        {"centroid", centroid.toJson()},
        {"fastgaussian", fastgaussian.toJson()}}}};
}

void ClusteringConfig::saveToFile(const std::string& config_path) const {
  std::ofstream file(config_path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot write clustering configuration file: " +
                             config_path);
  }

  file << toJson().dump(2);
  file.close();
}

bool ClusteringConfig::isClusteringAlgorithmSupported(
    const std::string& algorithm_name) {
  const auto supported = getSupportedClusteringAlgorithms();
  return std::find(supported.begin(), supported.end(), algorithm_name) !=
         supported.end();
}

bool ClusteringConfig::isPeakFittingAlgorithmSupported(
    const std::string& algorithm_name) {
  const auto supported = getSupportedPeakFittingAlgorithms();
  return std::find(supported.begin(), supported.end(), algorithm_name) !=
         supported.end();
}

std::vector<std::string> ClusteringConfig::getSupportedClusteringAlgorithms() {
  return {"abs"};  // Start with ABS only, add more later
}

std::vector<std::string> ClusteringConfig::getSupportedPeakFittingAlgorithms() {
  return {"centroid", "fastgaussian"};
}

void ClusteringConfig::merge(const ClusteringConfig& other) {
  clustering_algorithm = other.clustering_algorithm;
  peak_fitting_algorithm = other.peak_fitting_algorithm;
  enable_clustering = other.enable_clustering;
  enable_performance_logging = other.enable_performance_logging;

  // Merge algorithm configurations
  abs = other.abs;
  centroid = other.centroid;
  fastgaussian = other.fastgaussian;
}

std::string ClusteringConfig::summary() const {
  std::ostringstream ss;
  ss << "Clustering Configuration Summary:\n";
  ss << "  Clustering Algorithm: " << clustering_algorithm << "\n";
  ss << "  Peak Fitting Algorithm: " << peak_fitting_algorithm << "\n";
  ss << "  Clustering Enabled: " << (enable_clustering ? "Yes" : "No") << "\n";

  if (clustering_algorithm == "abs") {
    ss << "  ABS Parameters:\n";
    ss << "    Radius: " << abs.radius << " pixels\n";
    ss << "    Min Cluster Size: " << abs.min_cluster_size << " hits\n";
    ss << "    Correlation Window: " << abs.neutron_correlation_window
       << " ns\n";
    ss << "    Scan Interval: " << abs.scan_interval << " hits\n";
  }

  if (peak_fitting_algorithm == "centroid") {
    ss << "  Centroid Parameters:\n";
    ss << "    Super Resolution: " << centroid.super_resolution_factor << "x\n";
    ss << "    TOT Weighted: " << (centroid.weighted_by_tot ? "Yes" : "No")
       << "\n";
  } else if (peak_fitting_algorithm == "fastgaussian") {
    ss << "  FastGaussian Parameters:\n";
    ss << "    Super Resolution: " << fastgaussian.super_resolution_factor
       << "x\n";
    ss << "    Min Cluster Size: " << fastgaussian.min_cluster_size
       << " hits\n";
    ss << "    TOT Filter: " << (fastgaussian.tot_filter_fraction * 100)
       << "%\n";
  }

  return ss.str();
}

}  // namespace tdcsophiread