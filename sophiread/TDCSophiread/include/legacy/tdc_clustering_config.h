// TDCSophiread Clustering Configuration
// JSON-configurable parameters for clustering algorithms and peak fitting

#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

// Include algorithm headers for config definitions
#include "tdc_abs_clustering.h"
#include "tdc_centroid_fitting.h"
#include "tdc_graph_clustering.h"

namespace tdcsophiread {

/**
 * @brief Configuration for FastGaussian peak fitting algorithm
 */
struct FastGaussianConfig {
  double super_resolution_factor;  ///< Coordinate scaling factor (default: 8.0)
  uint16_t
      min_cluster_size;  ///< Minimum hits for Gaussian fitting (default: 8)
  double tot_filter_fraction;    ///< Fraction of lowest TOT hits to remove
                                 ///< (default: 0.5)
  uint16_t max_iterations;       ///< Maximum fitting iterations (default: 100)
  double convergence_tolerance;  ///< Convergence tolerance (default: 1e-6)

  /**
   * @brief Default constructor with recommended parameters
   */
  FastGaussianConfig()
      : super_resolution_factor(8.0),
        min_cluster_size(8),
        tot_filter_fraction(0.5),
        max_iterations(100),
        convergence_tolerance(1e-6) {}

  /**
   * @brief Validate configuration parameters
   * @throws std::invalid_argument if parameters are invalid
   */
  void validate() const;

  /**
   * @brief Load from JSON object
   * @param json JSON configuration
   */
  void fromJson(const nlohmann::json& json);

  /**
   * @brief Convert to JSON object
   * @return JSON representation
   */
  nlohmann::json toJson() const;
};

/**
 * @brief Complete clustering configuration
 *
 * Combines algorithm selection with algorithm-specific parameters.
 * Supports JSON loading/saving for persistent configuration.
 */
struct ClusteringConfig {
  std::string clustering_algorithm;    ///< Clustering algorithm name ("abs",
                                       ///< "dbscan", etc.)
  std::string peak_fitting_algorithm;  ///< Peak fitting algorithm ("centroid",
                                       ///< "fastgaussian")

  ABSConfig abs;                    ///< ABS algorithm configuration
  CentroidConfig centroid;          ///< Centroid algorithm configuration
  FastGaussianConfig fastgaussian;  ///< FastGaussian algorithm configuration
  GraphConfig graph;                ///< Graph algorithm configuration
  TemporalGraphConfig
      temporal_graph;  ///< Temporal graph algorithm configuration

  bool enable_clustering;  ///< Enable/disable clustering (default: true)
  bool
      enable_performance_logging;  ///< Log performance metrics (default: false)

  /**
   * @brief Default constructor with VENUS detector defaults
   */
  ClusteringConfig()
      : clustering_algorithm("abs"),
        peak_fitting_algorithm("centroid"),
        enable_clustering(true),
        enable_performance_logging(false) {}

  /**
   * @brief Create VENUS detector defaults
   * @return ClusteringConfig with optimized VENUS parameters
   */
  static ClusteringConfig venusDefaults();

  /**
   * @brief Load configuration from JSON file
   * @param config_path Path to JSON configuration file
   * @return ClusteringConfig loaded from file
   * @throws std::runtime_error if file cannot be read
   */
  static ClusteringConfig fromFile(const std::string& config_path);

  /**
   * @brief Load configuration from JSON object
   * @param json JSON configuration object
   * @return ClusteringConfig loaded from JSON
   */
  static ClusteringConfig fromJson(const nlohmann::json& json);

  /**
   * @brief Validate entire configuration
   * @throws std::invalid_argument if configuration is invalid
   */
  void validate() const;

  /**
   * @brief Convert to JSON object
   * @return Complete JSON representation
   */
  nlohmann::json toJson() const;

  /**
   * @brief Save configuration to JSON file
   * @param config_path Output file path
   * @throws std::runtime_error if file cannot be written
   */
  void saveToFile(const std::string& config_path) const;

  /**
   * @brief Get algorithm configuration by name
   * @param algorithm_name Name of clustering algorithm
   * @return Pointer to algorithm configuration
   * @throws std::invalid_argument if algorithm is unknown
   */
  const void* getAlgorithmConfig(const std::string& algorithm_name) const;

  /**
   * @brief Check if specified clustering algorithm is available
   * @param algorithm_name Algorithm name to check
   * @return True if algorithm is supported
   */
  static bool isClusteringAlgorithmSupported(const std::string& algorithm_name);

  /**
   * @brief Check if specified peak fitting algorithm is available
   * @param algorithm_name Algorithm name to check
   * @return True if algorithm is supported
   */
  static bool isPeakFittingAlgorithmSupported(
      const std::string& algorithm_name);

  /**
   * @brief Get list of supported clustering algorithms
   * @return Vector of algorithm names
   */
  static std::vector<std::string> getSupportedClusteringAlgorithms();

  /**
   * @brief Get list of supported peak fitting algorithms
   * @return Vector of algorithm names
   */
  static std::vector<std::string> getSupportedPeakFittingAlgorithms();

  /**
   * @brief Merge with another configuration (other takes precedence)
   * @param other Configuration to merge
   */
  void merge(const ClusteringConfig& other);

  /**
   * @brief Create configuration summary string
   * @return Human-readable configuration summary
   */
  std::string summary() const;
};

/**
 * @brief JSON schema validation for clustering configuration
 */
class ClusteringConfigValidator {
 public:
  /**
   * @brief Validate JSON against clustering configuration schema
   * @param json JSON object to validate
   * @return True if JSON is valid
   */
  static bool validate(const nlohmann::json& json);

  /**
   * @brief Get validation errors for JSON
   * @param json JSON object to validate
   * @return Vector of error messages (empty if valid)
   */
  static std::vector<std::string> getValidationErrors(
      const nlohmann::json& json);

  /**
   * @brief Get JSON schema for clustering configuration
   * @return JSON schema object
   */
  static nlohmann::json getSchema();
};

/**
 * @brief Configuration migration utilities
 *
 * Handles migration from older configuration formats to ensure
 * backward compatibility as the clustering system evolves.
 */
class ClusteringConfigMigration {
 public:
  /**
   * @brief Migrate configuration from FastSophiread format
   * @param fastsophiread_config FastSophiread JSON configuration
   * @return Equivalent TDCSophiread clustering configuration
   */
  static ClusteringConfig fromFastSophireadConfig(
      const nlohmann::json& fastsophiread_config);

  /**
   * @brief Detect configuration format version
   * @param json Configuration JSON
   * @return Configuration version string
   */
  static std::string detectConfigVersion(const nlohmann::json& json);

  /**
   * @brief Migrate configuration to current version
   * @param json Configuration JSON (potentially old format)
   * @return Migrated configuration in current format
   */
  static ClusteringConfig migrateToCurrentVersion(const nlohmann::json& json);
};

}  // namespace tdcsophiread

/**
 * @brief Example clustering configuration JSON structure:
 *
 * {
 *   "clustering": {
 *     "enable_clustering": true,
 *     "clustering_algorithm": "abs",
 *     "peak_fitting_algorithm": "centroid",
 *     "enable_performance_logging": false,
 *     "abs": {
 *       "radius": 5.0,
 *       "min_cluster_size": 1,
 *       "neutron_correlation_window": 75.0,
 *       "scan_interval": 100
 *     },
 *     "centroid": {
 *       "super_resolution_factor": 8.0,
 *       "weighted_by_tot": true,
 *       "min_tot_threshold": 0.0
 *     },
 *     "fastgaussian": {
 *       "super_resolution_factor": 8.0,
 *       "min_cluster_size": 8,
 *       "tot_filter_fraction": 0.5,
 *       "max_iterations": 100,
 *       "convergence_tolerance": 1e-6
 *     }
 *   }
 * }
 */