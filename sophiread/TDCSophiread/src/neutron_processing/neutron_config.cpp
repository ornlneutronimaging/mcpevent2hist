// TDCSophiread Neutron Processing Configuration Implementation
// Implementation of configuration validation and utility functions

#include "neutron_processing/neutron_config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace tdcsophiread {

// HitClusteringConfig validation
void HitClusteringConfig::validate() const {
  if (algorithm.empty()) {
    throw std::invalid_argument("Clustering algorithm name cannot be empty");
  }

  if (algorithm == "abs") {
    if (abs.radius <= 0.0) {
      throw std::invalid_argument("ABS clustering radius must be positive");
    }
    if (abs.neutron_correlation_window <= 0.0) {
      throw std::invalid_argument(
          "ABS neutron correlation window must be positive");
    }
    if (abs.scan_interval == 0) {
      throw std::invalid_argument("ABS scan interval must be positive");
    }
  }

  if (algorithm == "graph") {
    if (graph.radius <= 0.0) {
      throw std::invalid_argument("Graph clustering radius must be positive");
    }
    if (graph.grid_size <= 0.0) {
      throw std::invalid_argument("Graph grid size must be positive");
    }
  }
}

void HitClusteringConfig::fromJson(const nlohmann::json& json) {
  if (json.contains("algorithm")) {
    algorithm = json["algorithm"];
  }
  if (json.contains("abs")) {
    const auto& abs_json = json["abs"];
    if (abs_json.contains("radius")) abs.radius = abs_json["radius"];
    if (abs_json.contains("min_cluster_size"))
      abs.min_cluster_size = abs_json["min_cluster_size"];
    if (abs_json.contains("neutron_correlation_window"))
      abs.neutron_correlation_window = abs_json["neutron_correlation_window"];
    if (abs_json.contains("scan_interval"))
      abs.scan_interval = abs_json["scan_interval"];
  }
  if (json.contains("graph")) {
    const auto& graph_json = json["graph"];
    if (graph_json.contains("radius")) graph.radius = graph_json["radius"];
    if (graph_json.contains("min_cluster_size"))
      graph.min_cluster_size = graph_json["min_cluster_size"];
    if (graph_json.contains("grid_size"))
      graph.grid_size = graph_json["grid_size"];
    if (graph_json.contains("enable_spatial_hash"))
      graph.enable_spatial_hash = graph_json["enable_spatial_hash"];
    if (graph_json.contains("parallel_threshold"))
      graph.parallel_threshold = graph_json["parallel_threshold"];
  }
}

nlohmann::json HitClusteringConfig::toJson() const {
  nlohmann::json json;
  json["algorithm"] = algorithm;
  json["abs"] = {{"radius", abs.radius},
                 {"min_cluster_size", abs.min_cluster_size},
                 {"neutron_correlation_window", abs.neutron_correlation_window},
                 {"scan_interval", abs.scan_interval}};
  json["graph"] = {{"radius", graph.radius},
                   {"min_cluster_size", graph.min_cluster_size},
                   {"grid_size", graph.grid_size},
                   {"enable_spatial_hash", graph.enable_spatial_hash},
                   {"parallel_threshold", graph.parallel_threshold}};
  return json;
}

// NeutronExtractionConfig validation
void NeutronExtractionConfig::validate() const {
  if (algorithm.empty()) {
    throw std::invalid_argument("Extraction algorithm name cannot be empty");
  }

  if (super_resolution_factor <= 0.0) {
    throw std::invalid_argument("Super resolution factor must be positive");
  }

  if (algorithm == "gaussian") {
    if (gaussian_sigma_limit <= 0.0) {
      throw std::invalid_argument("Gaussian sigma limit must be positive");
    }
    if (max_iterations == 0) {
      throw std::invalid_argument("Max iterations must be positive");
    }
    if (convergence_tolerance <= 0.0) {
      throw std::invalid_argument("Convergence tolerance must be positive");
    }
  }
}

void NeutronExtractionConfig::fromJson(const nlohmann::json& json) {
  if (json.contains("algorithm")) algorithm = json["algorithm"];
  if (json.contains("super_resolution_factor"))
    super_resolution_factor = json["super_resolution_factor"];
  if (json.contains("weighted_by_tot"))
    weighted_by_tot = json["weighted_by_tot"];
  if (json.contains("min_tot_threshold"))
    min_tot_threshold = json["min_tot_threshold"];
  if (json.contains("gaussian_sigma_limit"))
    gaussian_sigma_limit = json["gaussian_sigma_limit"];
  if (json.contains("max_iterations")) max_iterations = json["max_iterations"];
  if (json.contains("convergence_tolerance"))
    convergence_tolerance = json["convergence_tolerance"];
}

nlohmann::json NeutronExtractionConfig::toJson() const {
  nlohmann::json json;
  json["algorithm"] = algorithm;
  json["super_resolution_factor"] = super_resolution_factor;
  json["weighted_by_tot"] = weighted_by_tot;
  json["min_tot_threshold"] = min_tot_threshold;
  json["gaussian_sigma_limit"] = gaussian_sigma_limit;
  json["max_iterations"] = max_iterations;
  json["convergence_tolerance"] = convergence_tolerance;
  return json;
}

// TemporalProcessingConfig validation
void TemporalProcessingConfig::validate() const {
  if (min_batch_size == 0) {
    throw std::invalid_argument("Minimum batch size must be positive");
  }
  if (max_batch_size < min_batch_size) {
    throw std::invalid_argument(
        "Maximum batch size must be >= minimum batch size");
  }
  if (overlap_factor < 0.0) {
    throw std::invalid_argument("Overlap factor cannot be negative");
  }
  if (deduplication_tolerance < 0.0) {
    throw std::invalid_argument("Deduplication tolerance cannot be negative");
  }
}

void TemporalProcessingConfig::fromJson(const nlohmann::json& json) {
  if (json.contains("num_workers")) num_workers = json["num_workers"];
  if (json.contains("min_batch_size")) min_batch_size = json["min_batch_size"];
  if (json.contains("max_batch_size")) max_batch_size = json["max_batch_size"];
  if (json.contains("overlap_factor")) overlap_factor = json["overlap_factor"];
  if (json.contains("enable_deduplication"))
    enable_deduplication = json["enable_deduplication"];
  if (json.contains("deduplication_tolerance"))
    deduplication_tolerance = json["deduplication_tolerance"];
  if (json.contains("enable_statistics"))
    enable_statistics = json["enable_statistics"];
}

nlohmann::json TemporalProcessingConfig::toJson() const {
  nlohmann::json json;
  json["num_workers"] = num_workers;
  json["min_batch_size"] = min_batch_size;
  json["max_batch_size"] = max_batch_size;
  json["overlap_factor"] = overlap_factor;
  json["enable_deduplication"] = enable_deduplication;
  json["deduplication_tolerance"] = deduplication_tolerance;
  json["enable_statistics"] = enable_statistics;
  return json;
}

// PerformanceConfig validation
void PerformanceConfig::validate() const {
  if (cache_line_size == 0 || (cache_line_size & (cache_line_size - 1)) != 0) {
    throw std::invalid_argument("Cache line size must be a power of 2");
  }
}

void PerformanceConfig::fromJson(const nlohmann::json& json) {
  if (json.contains("enable_memory_pools"))
    enable_memory_pools = json["enable_memory_pools"];
  if (json.contains("enable_vectorization"))
    enable_vectorization = json["enable_vectorization"];
  if (json.contains("enable_cache_optimization"))
    enable_cache_optimization = json["enable_cache_optimization"];
  if (json.contains("cache_line_size"))
    cache_line_size = json["cache_line_size"];
  if (json.contains("enable_profiling"))
    enable_profiling = json["enable_profiling"];
}

nlohmann::json PerformanceConfig::toJson() const {
  nlohmann::json json;
  json["enable_memory_pools"] = enable_memory_pools;
  json["enable_vectorization"] = enable_vectorization;
  json["enable_cache_optimization"] = enable_cache_optimization;
  json["cache_line_size"] = cache_line_size;
  json["enable_profiling"] = enable_profiling;
  return json;
}

// NeutronProcessingConfig implementation
NeutronProcessingConfig NeutronProcessingConfig::venusDefaults() {
  NeutronProcessingConfig config;
  config.clustering.algorithm = "abs";
  config.extraction.algorithm = "simple_centroid";
  return config;
}

NeutronProcessingConfig NeutronProcessingConfig::fromFile(
    const std::string& config_path) {
  std::ifstream file(config_path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open config file: " + config_path);
  }

  nlohmann::json json;
  file >> json;
  return fromJson(json);
}

NeutronProcessingConfig NeutronProcessingConfig::fromJson(
    const nlohmann::json& json) {
  NeutronProcessingConfig config;

  if (json.contains("clustering")) {
    config.clustering.fromJson(json["clustering"]);
  }
  if (json.contains("extraction")) {
    config.extraction.fromJson(json["extraction"]);
  }
  if (json.contains("temporal")) {
    config.temporal.fromJson(json["temporal"]);
  }
  if (json.contains("performance")) {
    config.performance.fromJson(json["performance"]);
  }

  return config;
}

void NeutronProcessingConfig::saveToFile(const std::string& config_path) const {
  std::ofstream file(config_path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot create config file: " + config_path);
  }

  file << toJson().dump(2);
}

nlohmann::json NeutronProcessingConfig::toJson() const {
  nlohmann::json json;
  json["clustering"] = clustering.toJson();
  json["extraction"] = extraction.toJson();
  json["temporal"] = temporal.toJson();
  json["performance"] = performance.toJson();
  return json;
}

void NeutronProcessingConfig::validate() const {
  clustering.validate();
  extraction.validate();
  temporal.validate();
  performance.validate();
}

std::string NeutronProcessingConfig::summary() const {
  std::ostringstream oss;
  oss << "Neutron Processing Configuration:\n";
  oss << "  Clustering: " << clustering.algorithm << "\n";
  oss << "  Extraction: " << extraction.algorithm << "\n";
  oss << "  Workers: "
      << (temporal.num_workers == 0 ? "auto"
                                    : std::to_string(temporal.num_workers))
      << "\n";
  oss << "  Batch size: " << temporal.min_batch_size << "-"
      << temporal.max_batch_size;
  return oss.str();
}

// Statistics implementations
std::string ProcessingStatistics::getSummary() const {
  std::ostringstream oss;
  oss << "Processing Statistics:\n";
  oss << "  Hits processed: " << total_hits_processed << "\n";
  oss << "  Neutrons produced: " << total_neutrons_produced << "\n";
  oss << "  Processing time: " << total_processing_time_ms << " ms\n";
  oss << "  Throughput: " << getHitsPerSecond() << " hits/sec\n";
  oss << "  Efficiency: " << (neutron_efficiency * 100.0) << "%";
  return oss.str();
}

}  // namespace tdcsophiread