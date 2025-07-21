// TDCSophiread Neutron Processing Factory Interfaces
// Factory pattern implementation for creating algorithm instances

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "neutron_processing/hit_clustering.h"
#include "neutron_processing/neutron_config.h"
#include "neutron_processing/neutron_extraction.h"
#include "neutron_processing/neutron_processing.h"

namespace tdcsophiread {

/**
 * @brief Factory for creating hit clustering algorithm instances
 *
 * Provides centralized creation of clustering algorithms with proper
 * configuration management. Supports runtime algorithm selection and
 * ensures consistent instantiation across the system.
 */
class HitClusteringFactory {
 public:
  /**
   * @brief Create hit clustering algorithm instance
   *
   * @param algorithm_name Name of the clustering algorithm
   * @param config Configuration for the algorithm
   * @return Unique pointer to configured clustering algorithm
   * @throws std::invalid_argument if algorithm name is unknown
   * @throws std::runtime_error if configuration is invalid
   *
   * Supported algorithms:
   * - "abs": Age-based spatial clustering (suitable for streaming)
   * - "graph": Graph-based clustering with spatial hashing
   * - "dbscan": DBSCAN density-based clustering
   */
  static std::unique_ptr<IHitClustering> create(
      const std::string& algorithm_name, const HitClusteringConfig& config);

  /**
   * @brief Get list of available clustering algorithms
   * @return Vector of algorithm names
   */
  static std::vector<std::string> getAvailableAlgorithms();

  /**
   * @brief Check if algorithm is available
   * @param algorithm_name Name to check
   * @return True if algorithm is supported
   */
  static bool isAlgorithmAvailable(const std::string& algorithm_name);

  /**
   * @brief Get algorithm description
   * @param algorithm_name Algorithm name
   * @return Human-readable description of the algorithm
   * @throws std::invalid_argument if algorithm name is unknown
   */
  static std::string getAlgorithmDescription(const std::string& algorithm_name);

  /**
   * @brief Get recommended configuration for algorithm
   * @param algorithm_name Algorithm name
   * @return Default configuration optimized for the algorithm
   * @throws std::invalid_argument if algorithm name is unknown
   */
  static HitClusteringConfig getRecommendedConfig(
      const std::string& algorithm_name);
};

/**
 * @brief Factory for creating neutron extraction algorithm instances
 *
 * Provides centralized creation of extraction algorithms that convert
 * clustered hits into neutron events with sub-pixel precision.
 */
class NeutronExtractionFactory {
 public:
  /**
   * @brief Create neutron extraction algorithm instance
   *
   * @param algorithm_name Name of the extraction algorithm
   * @param config Configuration for the algorithm
   * @return Unique pointer to configured extraction algorithm
   * @throws std::invalid_argument if algorithm name is unknown
   * @throws std::runtime_error if configuration is invalid
   *
   * Supported algorithms:
   * - "centroid": Weighted centroid calculation (fast, good for most cases)
   * - "gaussian": 2D Gaussian fitting (higher precision, more compute)
   * - "ml": Machine learning-based extraction (experimental)
   */
  static std::unique_ptr<INeutronExtraction> create(
      const std::string& algorithm_name, const NeutronExtractionConfig& config);

  /**
   * @brief Get list of available extraction algorithms
   * @return Vector of algorithm names
   */
  static std::vector<std::string> getAvailableAlgorithms();

  /**
   * @brief Check if algorithm is available
   * @param algorithm_name Name to check
   * @return True if algorithm is supported
   */
  static bool isAlgorithmAvailable(const std::string& algorithm_name);

  /**
   * @brief Get algorithm description
   * @param algorithm_name Algorithm name
   * @return Human-readable description of the algorithm
   * @throws std::invalid_argument if algorithm name is unknown
   */
  static std::string getAlgorithmDescription(const std::string& algorithm_name);

  /**
   * @brief Get recommended configuration for algorithm
   * @param algorithm_name Algorithm name
   * @return Default configuration optimized for the algorithm
   * @throws std::invalid_argument if algorithm name is unknown
   */
  static NeutronExtractionConfig getRecommendedConfig(
      const std::string& algorithm_name);
};

/**
 * @brief Factory for creating complete neutron processor instances
 *
 * Provides high-level creation of neutron processors that combine
 * clustering and extraction in various processing strategies.
 */
class NeutronProcessorFactory {
 public:
  /**
   * @brief Create neutron processor instance
   *
   * @param config Complete neutron processing configuration
   * @return Unique pointer to configured neutron processor
   * @throws std::invalid_argument if algorithms are unknown
   * @throws std::runtime_error if configuration is invalid
   *
   * Processor selection based on configuration:
   * - Single-threaded: BasicNeutronProcessor
   * - Multi-threaded: TemporalNeutronProcessor with worker pool
   */
  static std::unique_ptr<INeutronProcessor> create(
      const NeutronProcessingConfig& config);

  /**
   * @brief Create processor with specific implementation
   *
   * @param processor_type Type of processor to create
   * @param config Complete neutron processing configuration
   * @return Unique pointer to configured neutron processor
   * @throws std::invalid_argument if processor type is unknown
   *
   * Supported processor types:
   * - "basic": Single-threaded BasicNeutronProcessor
   * - "temporal": Multi-threaded TemporalNeutronProcessor
   */
  static std::unique_ptr<INeutronProcessor> create(
      const std::string& processor_type, const NeutronProcessingConfig& config);

  /**
   * @brief Get list of available processor types
   * @return Vector of processor type names
   */
  static std::vector<std::string> getAvailableProcessorTypes();

  /**
   * @brief Get processor type description
   * @param processor_type Processor type name
   * @return Human-readable description of the processor
   * @throws std::invalid_argument if processor type is unknown
   */
  static std::string getProcessorDescription(const std::string& processor_type);
};

/**
 * @brief Algorithm registry for runtime discovery and validation
 *
 * Central registry that maintains information about all available
 * algorithms and their capabilities. Used by factories and for
 * runtime introspection.
 */
class AlgorithmRegistry {
 public:
  /**
   * @brief Algorithm information structure
   */
  struct AlgorithmInfo {
    std::string name;                   ///< Algorithm identifier
    std::string display_name;           ///< Human-readable name
    std::string description;            ///< Algorithm description
    std::string category;               ///< "clustering" or "extraction"
    bool is_available;                  ///< Whether algorithm is compiled in
    std::vector<std::string> features;  ///< Special features/capabilities

    AlgorithmInfo(const std::string& name_, const std::string& display_name_,
                  const std::string& description_, const std::string& category_,
                  bool is_available_ = true)
        : name(name_),
          display_name(display_name_),
          description(description_),
          category(category_),
          is_available(is_available_) {}
  };

  /**
   * @brief Get information about all algorithms
   * @return Vector of algorithm information
   */
  static std::vector<AlgorithmInfo> getAllAlgorithms();

  /**
   * @brief Get algorithms by category
   * @param category "clustering" or "extraction"
   * @return Vector of algorithm information for the category
   */
  static std::vector<AlgorithmInfo> getAlgorithmsByCategory(
      const std::string& category);

  /**
   * @brief Get algorithm information
   * @param algorithm_name Algorithm name
   * @return Algorithm information
   * @throws std::invalid_argument if algorithm is unknown
   */
  static AlgorithmInfo getAlgorithmInfo(const std::string& algorithm_name);

  /**
   * @brief Validate algorithm combination
   * @param clustering_algorithm Clustering algorithm name
   * @param extraction_algorithm Extraction algorithm name
   * @return True if combination is supported
   */
  static bool validateAlgorithmCombination(
      const std::string& clustering_algorithm,
      const std::string& extraction_algorithm);
};

/**
 * @brief Configuration builder for easy setup
 *
 * Provides fluent interface for building neutron processing configurations
 * with sensible defaults and validation.
 */
class ConfigurationBuilder {
 private:
  NeutronProcessingConfig config_;

 public:
  /**
   * @brief Start with VENUS detector defaults
   */
  ConfigurationBuilder();

  /**
   * @brief Start with custom base configuration
   */
  explicit ConfigurationBuilder(const NeutronProcessingConfig& base_config);

  /**
   * @brief Set clustering algorithm
   * @param algorithm_name Name of clustering algorithm
   * @return Reference to builder for chaining
   */
  ConfigurationBuilder& withClusteringAlgorithm(
      const std::string& algorithm_name);

  /**
   * @brief Set extraction algorithm
   * @param algorithm_name Name of extraction algorithm
   * @return Reference to builder for chaining
   */
  ConfigurationBuilder& withExtractionAlgorithm(
      const std::string& algorithm_name);

  /**
   * @brief Set clustering radius
   * @param radius Spatial clustering radius in pixels
   * @return Reference to builder for chaining
   */
  ConfigurationBuilder& withClusteringRadius(double radius);

  /**
   * @brief Set correlation window
   * @param window_ns Temporal correlation window in nanoseconds
   * @return Reference to builder for chaining
   */
  ConfigurationBuilder& withCorrelationWindow(double window_ns);

  /**
   * @brief Set super-resolution factor
   * @param factor Coordinate scaling factor for sub-pixel precision
   * @return Reference to builder for chaining
   */
  ConfigurationBuilder& withSuperResolution(double factor);

  /**
   * @brief Set number of worker threads
   * @param num_workers Number of worker threads (0 = auto-detect)
   * @return Reference to builder for chaining
   */
  ConfigurationBuilder& withWorkers(size_t num_workers);

  /**
   * @brief Enable/disable deduplication
   * @param enable Whether to remove duplicate neutrons in overlap regions
   * @return Reference to builder for chaining
   */
  ConfigurationBuilder& withDeduplication(bool enable);

  /**
   * @brief Set batch size limits
   * @param min_size Minimum hits per batch
   * @param max_size Maximum hits per batch
   * @return Reference to builder for chaining
   */
  ConfigurationBuilder& withBatchSizes(size_t min_size, size_t max_size);

  /**
   * @brief Enable performance optimizations
   * @param enable_memory_pools Use memory pools
   * @param enable_vectorization Use SIMD optimizations
   * @return Reference to builder for chaining
   */
  ConfigurationBuilder& withPerformanceOptimizations(
      bool enable_memory_pools = true, bool enable_vectorization = true);

  /**
   * @brief Build final configuration
   * @return Complete validated neutron processing configuration
   * @throws std::runtime_error if configuration is invalid
   */
  NeutronProcessingConfig build();

  /**
   * @brief Validate current configuration
   * @return True if configuration is valid
   */
  bool validate() const;

  /**
   * @brief Get configuration summary
   * @return Human-readable summary of current configuration
   */
  std::string summary() const;
};

/**
 * @brief Utility functions for factory operations
 */
namespace FactoryUtils {

/**
 * @brief Create complete processing pipeline with defaults
 * @param clustering_algorithm Clustering algorithm name
 * @param extraction_algorithm Extraction algorithm name
 * @return Configured neutron processor ready for use
 */
std::unique_ptr<INeutronProcessor> createDefaultProcessor(
    const std::string& clustering_algorithm = "abs",
    const std::string& extraction_algorithm = "centroid");

/**
 * @brief Create high-performance processor for production
 * @param num_workers Number of worker threads (0 = auto-detect)
 * @return Optimized processor for high-throughput production use
 */
std::unique_ptr<INeutronProcessor> createProductionProcessor(
    size_t num_workers = 0);

/**
 * @brief Create processor optimized for diagnostics
 * @return Processor with enhanced diagnostics and validation
 */
std::unique_ptr<INeutronProcessor> createDiagnosticProcessor();

/**
 * @brief Validate algorithm availability at runtime
 * @param clustering_algorithm Clustering algorithm name
 * @param extraction_algorithm Extraction algorithm name
 * @throws std::runtime_error if algorithms are not available
 */
void validateAlgorithmAvailability(const std::string& clustering_algorithm,
                                   const std::string& extraction_algorithm);

/**
 * @brief Get optimal configuration for hardware
 * @param num_cores Number of CPU cores available
 * @param memory_gb Available memory in GB
 * @return Configuration optimized for the hardware
 */
NeutronProcessingConfig getOptimalConfigForHardware(size_t num_cores,
                                                    size_t memory_gb);

/**
 * @brief Estimate processing performance
 * @param config Processing configuration
 * @param num_hits Expected number of hits
 * @return Estimated processing rate in hits/second
 */
double estimateProcessingRate(const NeutronProcessingConfig& config,
                              size_t num_hits);

}  // namespace FactoryUtils

}  // namespace tdcsophiread