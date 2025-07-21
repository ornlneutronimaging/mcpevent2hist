// TDCSophiread Neutron Processing Configuration
// Configuration structures for the new neutron processing architecture

#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace tdcsophiread {

// Forward declarations for statistics structures
struct ClusteringStatistics;
struct ExtractionStatistics;
struct ProcessingStatistics;

/**
 * @brief Algorithm-specific configuration for ABS clustering
 */
struct ABSConfig {
  double radius;  ///< Spatial clustering radius in pixels (default: 5.0)
  uint16_t min_cluster_size;          ///< Minimum hits per cluster (default: 1)
  double neutron_correlation_window;  ///< Temporal correlation window in
                                      ///< nanoseconds (default: 75.0)
  size_t scan_interval;  ///< Scan for aged buckets every N hits (default: 100)
  size_t
      pre_allocate_buckets;  ///< Pre-allocate bucket pool size (default: 1000)

  ABSConfig()
      : radius(5.0),
        min_cluster_size(1),
        neutron_correlation_window(75.0),
        scan_interval(100),
        pre_allocate_buckets(1000) {}
};

/**
 * @brief Algorithm-specific configuration for Graph clustering
 */
struct GraphConfig {
  double radius;  ///< Spatial clustering radius in pixels (default: 5.0)
  uint16_t min_cluster_size;  ///< Minimum hits per cluster (default: 1)
  double grid_size;           ///< Spatial grid size for hashing (default: 5.0)
  bool enable_spatial_hash;   ///< Enable spatial hash optimization (default:
                              ///< true)
  size_t parallel_threshold;  ///< Minimum hits for parallel processing
                              ///< (default: 100000)

  GraphConfig()
      : radius(5.0),
        min_cluster_size(1),
        grid_size(5.0),
        enable_spatial_hash(true),
        parallel_threshold(100000) {}
};

/**
 * @brief Algorithm-specific configuration for DBSCAN clustering
 */
struct DBSCANConfig {
  double epsilon;  ///< Maximum distance between neighbor points (default: 5.0)
  uint16_t min_points;  ///< Minimum points to form dense region (default: 4)
  double neutron_correlation_window;  ///< Temporal correlation window in
                                      ///< nanoseconds (default: 75.0)
  double grid_size;  ///< Spatial grid size for neighbor search (default: 5.0)

  DBSCANConfig()
      : epsilon(5.0),
        min_points(4),
        neutron_correlation_window(75.0),
        grid_size(5.0) {}
};

/**
 * @brief Configuration for grid-based clustering algorithm
 */
struct GridConfig {
  uint16_t grid_cols;          ///< Number of grid columns (default: 32)
  uint16_t grid_rows;          ///< Number of grid rows (default: 32)
  double connection_distance;  ///< Max distance to connect hits (default: 4.0)
  double neutron_correlation_window;  ///< Temporal correlation window in
                                      ///< nanoseconds (default: 75.0)
  bool merge_adjacent_cells;          ///< Merge clusters across cell boundaries
                                      ///< (default: true)

  GridConfig()
      : grid_cols(32),
        grid_rows(32),
        connection_distance(4.0),
        neutron_correlation_window(75.0),
        merge_adjacent_cells(true) {}
};

/**
 * @brief Configuration for hit clustering algorithms
 */
struct HitClusteringConfig {
  std::string algorithm;  ///< Algorithm name ("abs", "graph", "dbscan", "grid")

  // Algorithm-specific configurations
  ABSConfig abs;        ///< ABS algorithm configuration
  GraphConfig graph;    ///< Graph algorithm configuration
  DBSCANConfig dbscan;  ///< DBSCAN algorithm configuration
  GridConfig grid;      ///< Grid algorithm configuration

  /**
   * @brief Default constructor with VENUS detector defaults
   */
  HitClusteringConfig() : algorithm("abs"), abs(), graph(), dbscan(), grid() {}

  /**
   * @brief Factory method for VENUS detector defaults
   * @return Configuration with VENUS-specific settings
   */
  static HitClusteringConfig venusDefaults() {
    HitClusteringConfig config;
    config.algorithm = "abs";
    config.abs = ABSConfig();  // Already has VENUS defaults
    return config;
  }

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
 * @brief Configuration for neutron extraction algorithms
 */
struct NeutronExtractionConfig {
  std::string algorithm;  ///< Algorithm name ("centroid", "gaussian", "ml")

  // Common extraction parameters
  double super_resolution_factor;  ///< Coordinate scaling factor for sub-pixel
                                   ///< precision (default: 8.0)
  bool weighted_by_tot;  ///< Use TOT weighting for calculations (default: true)
  uint16_t min_tot_threshold;  ///< Minimum TOT for hit inclusion (default: 0 =
                               ///< disabled)

  // Algorithm-specific parameters
  double gaussian_sigma_limit;  ///< Gaussian: Maximum sigma for fitting
                                ///< (default: 3.0)
  size_t max_iterations;  ///< Gaussian/ML: Maximum fitting iterations (default:
                          ///< 100)
  double convergence_tolerance;  ///< Gaussian/ML: Convergence tolerance
                                 ///< (default: 1e-6)

  /**
   * @brief Default constructor with VENUS detector defaults
   */
  NeutronExtractionConfig()
      : algorithm("centroid"),
        super_resolution_factor(8.0),
        weighted_by_tot(true),
        min_tot_threshold(0),  // Disabled by default
        gaussian_sigma_limit(3.0),
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
 * @brief Configuration for temporal processing (parallel batching)
 */
struct TemporalProcessingConfig {
  size_t
      num_workers;  ///< Number of worker threads (0 = auto-detect, default: 0)
  size_t min_batch_size;  ///< Minimum hits per batch (default: 1000)
  size_t max_batch_size;  ///< Maximum hits per batch (default: 100000)
  double overlap_factor;  ///< Overlap size multiplier (default: 3.0 for 3σ)
  bool enable_deduplication;  ///< Remove duplicate neutrons in overlap regions
                              ///< (default: false)
  double deduplication_tolerance;  ///< Spatial tolerance for duplicate
                                   ///< detection in pixels (default: 1.0)
  bool enable_statistics;  ///< Collect detailed processing statistics (default:
                           ///< true)

  /**
   * @brief Default constructor with production defaults
   */
  TemporalProcessingConfig()
      : num_workers(0),
        min_batch_size(1000),
        max_batch_size(100000),
        overlap_factor(3.0),
        enable_deduplication(false),  // Disabled by default - poor ROI
        deduplication_tolerance(1.0),
        enable_statistics(true) {}

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
 * @brief Performance configuration settings
 */
struct PerformanceConfig {
  bool enable_memory_pools;   ///< Use memory pools for performance (default:
                              ///< true)
  bool enable_vectorization;  ///< Enable SIMD optimizations where available
                              ///< (default: true)
  bool enable_cache_optimization;  ///< Optimize for cache performance (default:
                                   ///< true)
  size_t cache_line_size;  ///< Target cache line size in bytes (default: 64)
  bool enable_profiling;   ///< Enable detailed performance profiling (default:
                           ///< false)

  /**
   * @brief Default constructor with performance defaults
   */
  PerformanceConfig()
      : enable_memory_pools(true),
        enable_vectorization(true),
        enable_cache_optimization(true),
        cache_line_size(64),
        enable_profiling(false) {}

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
 * @brief Main neutron processing configuration
 *
 * Contains all configuration needed for the complete hits→neutrons pipeline
 */
struct NeutronProcessingConfig {
  HitClusteringConfig clustering;      ///< Hit clustering configuration
  NeutronExtractionConfig extraction;  ///< Neutron extraction configuration
  TemporalProcessingConfig temporal;   ///< Temporal processing configuration
  PerformanceConfig performance;       ///< Performance settings

  /**
   * @brief Create VENUS detector default configuration
   */
  static NeutronProcessingConfig venusDefaults();

  /**
   * @brief Load configuration from JSON file
   * @param config_path Path to JSON configuration file
   * @return Loaded configuration
   * @throws std::runtime_error if file cannot be read or parsed
   */
  static NeutronProcessingConfig fromFile(const std::string& config_path);

  /**
   * @brief Load configuration from JSON object
   * @param json JSON configuration object
   * @return Loaded configuration
   */
  static NeutronProcessingConfig fromJson(const nlohmann::json& json);

  /**
   * @brief Save configuration to JSON file
   * @param config_path Path to save JSON configuration
   * @throws std::runtime_error if file cannot be written
   */
  void saveToFile(const std::string& config_path) const;

  /**
   * @brief Convert configuration to JSON object
   * @return JSON representation
   */
  nlohmann::json toJson() const;

  /**
   * @brief Validate all configuration parameters
   * @throws std::invalid_argument if any parameters are invalid
   */
  void validate() const;

  /**
   * @brief Get configuration summary as human-readable string
   * @return Summary string
   */
  std::string summary() const;
};

// Statistics structures for performance monitoring

/**
 * @brief Statistics for hit clustering operations
 */
struct ClusteringStatistics {
  size_t total_hits_processed = 0;  ///< Total hits processed
  size_t total_clusters = 0;        ///< Total clusters found
  size_t unclustered_hits = 0;      ///< Hits not assigned to clusters
  double processing_time_ms = 0.0;  ///< Processing time in milliseconds
  double mean_cluster_size = 0.0;   ///< Average hits per cluster
  size_t max_cluster_size = 0;      ///< Largest cluster size
  size_t buckets_created = 0;       ///< ABS: Total buckets created
  size_t buckets_aged_out = 0;      ///< ABS: Buckets aged out
  double memory_usage_mb = 0.0;     ///< Memory usage in MB
};

/**
 * @brief Statistics for neutron extraction operations
 */
struct ExtractionStatistics {
  size_t total_hits_processed = 0;      ///< Total hits processed
  size_t total_clusters_processed = 0;  ///< Total clusters processed
  size_t total_neutrons_extracted = 0;  ///< Total neutrons extracted
  size_t rejected_clusters = 0;         ///< Clusters rejected during extraction
  double processing_time_ms = 0.0;      ///< Processing time in milliseconds
  double mean_hits_per_neutron = 0.0;   ///< Average hits per neutron
  double extraction_efficiency = 0.0;   ///< Neutrons/clusters ratio
  size_t single_hit_neutrons = 0;       ///< Neutrons from single hits
  size_t multi_hit_neutrons = 0;        ///< Neutrons from multiple hits
};

/**
 * @brief Statistics for complete neutron processing pipeline
 */
struct ProcessingStatistics {
  // Input/output counts
  size_t total_hits_processed = 0;     ///< Total hits processed
  size_t total_neutrons_produced = 0;  ///< Total neutrons produced
  size_t total_neutrons_found = 0;     ///< Alias for total_neutrons_produced
  size_t total_clusters_found = 0;     ///< Total clusters found

  // Timing breakdown
  double total_processing_time_ms = 0.0;  ///< Total processing time
  double processing_time_ms = 0.0;   ///< Alias for total_processing_time_ms
  double analysis_time_ms = 0.0;     ///< Time for hit distribution analysis
  double batching_time_ms = 0.0;     ///< Time for batch creation
  double clustering_time_ms = 0.0;   ///< Time for clustering
  double extraction_time_ms = 0.0;   ///< Time for neutron extraction
  double aggregation_time_ms = 0.0;  ///< Time for result aggregation

  // Parallel processing metrics
  size_t num_workers_used = 1;       ///< Number of workers used
  size_t num_batches_created = 1;    ///< Number of batches created
  double parallel_efficiency = 1.0;  ///< Parallel efficiency factor
  double load_balance_factor = 1.0;  ///< Load balancing factor

  // Memory metrics
  double peak_memory_usage_mb = 0.0;  ///< Peak memory usage in MB
  double memory_per_worker_mb = 0.0;  ///< Memory per worker in MB

  // Quality metrics
  double neutron_efficiency = 0.0;  ///< Neutrons per hit ratio
  double mean_cluster_size = 0.0;   ///< Average cluster size
  size_t clusters_rejected = 0;     ///< Clusters rejected during extraction
  size_t duplicate_neutrons_removed =
      0;  ///< Duplicates removed during deduplication

  // Additional fields for compatibility
  double hits_per_second = 0.0;      ///< Hits processed per second
  double neutrons_per_second = 0.0;  ///< Neutrons produced per second
  size_t num_batches =
      0;  ///< Number of batches (alias for num_batches_created)
  double avg_batch_size = 0.0;  ///< Average batch size

  /**
   * @brief Calculate hits per second throughput
   */
  double getHitsPerSecond() const {
    return (total_processing_time_ms > 0.0)
               ? (total_hits_processed * 1000.0 / total_processing_time_ms)
               : 0.0;
  }

  /**
   * @brief Get processing efficiency summary
   */
  std::string getSummary() const;
};

}  // namespace tdcsophiread