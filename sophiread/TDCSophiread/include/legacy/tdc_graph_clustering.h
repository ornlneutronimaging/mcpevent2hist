// TDCSophiread Graph-Based Clustering Algorithm
// Parallel spatial-temporal clustering using graph theory and spatial hashing
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "tdc_centroid_fitting.h"
#include "tdc_clustering.h"
// Forward declarations
struct ClusteringConfig;
#include "tdc_hit.h"
#include "tdc_neutron.h"

namespace tdcsophiread {

/**
 * @brief Configuration for Graph-based clustering algorithm
 */
struct GraphConfig {
  double radius;  ///< Spatial clustering radius in pixels (default: 5.0)
  uint16_t min_cluster_size;          ///< Minimum hits per cluster (default: 1)
  double neutron_correlation_window;  ///< Neutron temporal correlation window
                                      ///< in nanoseconds (default: 75.0)
  double grid_size;  ///< Spatial grid size for hashing (default: 5.0, same as
                     ///< radius)
  bool enable_spatial_hash;   ///< Enable spatial hash optimization (default:
                              ///< true)
  size_t parallel_threshold;  ///< Minimum hits for parallel processing
                              ///< (default: 100000)

  /**
   * @brief Default constructor with VENUS detector defaults
   */
  GraphConfig()
      : radius(5.0),
        min_cluster_size(1),
        neutron_correlation_window(75.0),
        grid_size(5.0),
        enable_spatial_hash(true),
        parallel_threshold(100000) {}

  /**
   * @brief Validate configuration parameters
   * @throws std::invalid_argument if parameters are invalid
   */
  void validate() const;
};

/**
 * @brief Configuration for temporal graph clustering processor
 */
struct TemporalGraphConfig {
  GraphConfig graph_config;        ///< Base graph clustering configuration
  CentroidConfig centroid_config;  ///< Centroid peak fitting configuration
  size_t num_workers;        ///< Number of worker threads (0 = auto-detect)
  size_t min_batch_size;     ///< Minimum hits per batch (default: 1000)
  size_t max_batch_size;     ///< Maximum hits per batch (default: 100000)
  double overlap_factor;     ///< Overlap size multiplier (default: 3.0 for 3σ)
  bool enable_memory_pools;  ///< Enable per-worker memory pools (default: true)
  bool enable_temporal_aging;  ///< Enable temporal aging within batches
                               ///< (default: true)

  /**
   * @brief Default constructor with production defaults
   */
  TemporalGraphConfig()
      : graph_config(),
        centroid_config(),
        num_workers(0),
        min_batch_size(1000),
        max_batch_size(100000),
        overlap_factor(3.0),
        enable_memory_pools(true),
        enable_temporal_aging(true) {}

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
 * @brief Statistical information about hit distribution for temporal batching
 */
struct BatchStats {
  double mean_hits_per_window;  ///< Average hits within correlation window
  double std_hits_per_window;   ///< Standard deviation of hits per window
  uint32_t optimal_window_tof;  ///< Optimal window size in TOF units (25ns)
  size_t overlap_size;  ///< Overlap size in hits (3σ for boundary handling)
  size_t num_pulses_analyzed;  ///< Number of complete pulses analyzed
  uint32_t pulse_period_tof;   ///< TOF period between pulses (25ns units)
  size_t total_hits_analyzed;  ///< Total hits used for statistics

  /**
   * @brief Default constructor
   */
  BatchStats()
      : mean_hits_per_window(0.0),
        std_hits_per_window(0.0),
        optimal_window_tof(0),
        overlap_size(0),
        num_pulses_analyzed(0),
        pulse_period_tof(0),
        total_hits_analyzed(0) {}
};

/**
 * @brief Temporal batch definition for zero-copy processing
 */
struct HitBatch {
  const std::vector<TDCHit>*
      hits_ptr;               ///< Reference to original hits vector (zero-copy)
  size_t start_index;         ///< Batch start index (inclusive)
  size_t end_index;           ///< Batch end index (exclusive)
  size_t overlap_start;       ///< Overlap region start index
  size_t overlap_end;         ///< Overlap region end index
  uint32_t tof_window_start;  ///< TOF range start for this batch
  uint32_t tof_window_end;    ///< TOF range end for this batch

  /**
   * @brief Default constructor
   */
  HitBatch()
      : hits_ptr(nullptr),
        start_index(0),
        end_index(0),
        overlap_start(0),
        overlap_end(0),
        tof_window_start(0),
        tof_window_end(0) {}

  /**
   * @brief Get number of hits in this batch
   */
  size_t size() const { return end_index - start_index; }

  /**
   * @brief Check if batch is valid
   */
  bool isValid() const {
    return hits_ptr != nullptr && start_index < end_index;
  }
};

/**
 * @brief Graph-based clustering algorithm implementation
 *
 * Physics-correct spatial-temporal clustering using graph theory.
 * Designed for neutron detection with MCP detector characteristics:
 * - Each neutron creates small cloud of hits (2-3 hits within ~5 pixels, same
 * 75ns window)
 * - Results in sparse graph with small, well-separated connected components
 * - Highly parallelizable unlike sequential ABS algorithm
 *
 * Algorithm Overview:
 * 1. Build spatial hash table for efficient neighbor finding
 * 2. Construct graph where hits are nodes, edges connect spatial-temporal
 * neighbors
 * 3. Find connected components (each becomes a cluster)
 * 4. Assign cluster IDs to hits
 *
 * Performance Characteristics:
 * - O(N × k) where k ≈ 3 average neighbors per hit (vs ABS O(N × 1000))
 * - Sparse graph structure ideal for parallelization
 * - Memory: Additional ~1.5GB spatial index for 100M hits
 * - Scalability: Linear with thread count up to 32 threads on EPYC
 */
class GraphClustering : public IClusteringAlgorithm {
 public:
  /**
   * @brief Constructor with configuration
   * @param config Graph algorithm parameters
   */
  explicit GraphClustering(const GraphConfig& config);

  // IClusteringAlgorithm interface implementation
  void configure(const ClusteringConfig& config) override;
  size_t fit(std::vector<TDCHit>& hits) override;
  size_t fit(std::vector<TDCHit>::iterator begin,
             std::vector<TDCHit>::iterator end) override;
  const std::vector<int>& getClusterLabels() const override;
  std::string getName() const override;
  void reset() override;
  size_t getLastHitCount() const override;

  /**
   * @brief Get clustering statistics from last fit() call
   * @return Performance and quality metrics
   */
  struct ClusteringStats {
    size_t total_hits;                ///< Number of hits processed
    size_t total_clusters;            ///< Number of valid clusters created
    size_t total_edges;               ///< Number of edges in graph
    size_t unclustered_hits;          ///< Hits not assigned to any cluster
    double mean_cluster_size;         ///< Average hits per valid cluster
    double processing_time_ms;        ///< Wall-clock processing time
    double graph_construction_ms;     ///< Time for graph construction
    double connected_components_ms;   ///< Time for connected components
    size_t spatial_hash_buckets;      ///< Number of spatial hash buckets used
    double spatial_hash_load_factor;  ///< Load factor of spatial hash table
  };

  /**
   * @brief Get statistics from last clustering operation
   * @return Detailed clustering statistics
   */
  ClusteringStats getStatistics() const;

  /**
   * @brief Update algorithm configuration
   * @param config New graph parameters
   */
  void updateConfig(const GraphConfig& config);

  // Temporal batching static analysis functions

  /**
   * @brief Analyze hit distribution for temporal batching
   * @param hits Hit vector to analyze
   * @param num_pulses Number of pulses to analyze (default: 2)
   * @param correlation_window Correlation window in nanoseconds (default: 75.0)
   * @return Statistical information for batch creation
   */
  static BatchStats analyzeHitDistribution(const std::vector<TDCHit>& hits,
                                           int num_pulses = 2,
                                           double correlation_window = 75.0);

  /**
   * @brief Create temporal batches from hit vector
   * @param hits Hit vector to partition
   * @param stats Statistical information from analysis
   * @return Vector of temporal batches for parallel processing
   */
  static std::vector<HitBatch> createStatisticalBatches(
      const std::vector<TDCHit>& hits, const BatchStats& stats);

  /**
   * @brief Process a single temporal batch
   * @param batch Temporal batch to process
   * @param config Graph clustering configuration
   * @return Vector of neutrons from this batch
   */
  static std::vector<TDCNeutron> processBatch(const HitBatch& batch,
                                              const GraphConfig& config);

 private:
  // Configuration
  GraphConfig config_;

  // Hit data reference
  std::vector<TDCHit>* hits_ptr_;

  // Spatial hashing
  using SpatialKey = uint32_t;
  using HitIndex = size_t;
  std::unordered_map<SpatialKey, std::vector<HitIndex>> spatial_hash_;

  // Streaming Union-Find data structures (no edge storage)
  std::unique_ptr<std::atomic<HitIndex>[]>
      parent_;  ///< Union-Find parent array (atomic for thread-safety)
  std::unique_ptr<std::atomic<int>[]>
      rank_;  ///< Union-Find rank array (atomic for thread-safety)

  // Sequential Union-Find for small datasets (no atomics)
  std::unique_ptr<HitIndex[]>
      seq_parent_;                   ///< Sequential Union-Find parent array
  std::unique_ptr<int[]> seq_rank_;  ///< Sequential Union-Find rank array

  size_t union_find_size_;  ///< Size of Union-Find arrays
  std::atomic<size_t>
      edge_count_;  ///< Count of edges processed (for statistics)

  // Output data
  std::vector<int> cluster_labels_;  ///< Cluster assignment for each hit
  int32_t next_cluster_label_;       ///< Next available cluster label

  // Statistics
  mutable ClusteringStats stats_;
  mutable std::chrono::high_resolution_clock::time_point start_time_;

  /**
   * @brief Encode spatial coordinates to hash key
   * @param x X coordinate
   * @param y Y coordinate
   * @return Spatial hash key
   */
  SpatialKey encodeSpatialKey(uint16_t x, uint16_t y) const;

  /**
   * @brief Get neighboring grid cells for a given position
   * @param x X coordinate
   * @param y Y coordinate
   * @return Vector of neighboring spatial keys
   */
  std::vector<SpatialKey> getNeighborCells(uint16_t x, uint16_t y) const;

  /**
   * @brief Build spatial hash table from hits
   */
  void buildSpatialHash();

  /**
   * @brief Process connections using streaming Union-Find (no edge storage)
   */
  void constructGraph();

  /**
   * @brief Sequential graph construction for small datasets
   */
  void constructGraphSequential();

  /**
   * @brief Initialize Union-Find data structures
   */
  void initializeUnionFind();

  /**
   * @brief Sequential Union-Find initialization for small datasets
   */
  void initializeUnionFindSequential();

  /**
   * @brief Atomic find operation for Union-Find with path compression
   * @param x Hit index to find root for
   * @return Root of the set containing x
   */
  HitIndex atomicFind(HitIndex x);

  /**
   * @brief Atomic unite operation for Union-Find
   * @param x First hit index
   * @param y Second hit index
   */
  void atomicUnite(HitIndex x, HitIndex y);

  /**
   * @brief Sequential find operation for Union-Find with path compression
   * @param x Hit index to find root for
   * @return Root of the set containing x
   */
  HitIndex sequentialFind(HitIndex x);

  /**
   * @brief Sequential unite operation for Union-Find
   * @param x First hit index
   * @param y Second hit index
   */
  void sequentialUnite(HitIndex x, HitIndex y);

  /**
   * @brief Find connected components using Union-Find algorithm
   * @return Number of connected components found
   */
  size_t findConnectedComponents();

  /**
   * @brief Check if two hits should be connected by an edge
   * @param hit1 First hit
   * @param hit2 Second hit
   * @return True if hits satisfy spatial and temporal constraints
   */
  bool shouldConnect(const TDCHit& hit1, const TDCHit& hit2) const;

  /**
   * @brief Update clustering statistics
   */
  void updateStatistics();

  /**
   * @brief Clear all internal data structures for new clustering run
   */
  void clearInternalState();
};

/**
 * @brief Temporal graph clustering processor for parallel batch processing
 *
 * High-performance processor that uses temporal batching to achieve
 * production-scale clustering performance. Designed to leverage TPX3's temporal
 * structure for efficient parallel processing while maintaining physics
 * correctness.
 *
 * Key Features:
 * - Statistical temporal batching based on hit distribution analysis
 * - Zero-copy batch processing with overlap handling for boundary neutrons
 * - Worker pool architecture with per-worker memory management
 * - Streaming output with temporal aging to maintain bounded memory
 * - Linear scaling up to available core count
 *
 * Performance Targets:
 * - M2 Max (12 cores): 50-100M hits/sec
 * - EPYC 9174F (32 cores): 200-400M hits/sec
 */
class TemporalGraphClusteringProcessor {
 public:
  /**
   * @brief Processing statistics for performance monitoring
   */
  struct ProcessingStats {
    size_t total_hits_processed;     ///< Total hits processed
    size_t total_neutrons_produced;  ///< Total neutrons produced
    size_t num_batches_created;      ///< Number of temporal batches created
    size_t num_workers_used;         ///< Number of worker threads used
    double analysis_time_ms;         ///< Time for hit distribution analysis
    double batching_time_ms;         ///< Time for batch creation
    double processing_time_ms;       ///< Time for parallel batch processing
    double aggregation_time_ms;      ///< Time for result aggregation
    double total_time_ms;            ///< Total processing time
    double hits_per_second;          ///< Processing rate
    double neutron_efficiency;       ///< Neutrons per hit ratio

    /**
     * @brief Default constructor
     */
    ProcessingStats()
        : total_hits_processed(0),
          total_neutrons_produced(0),
          num_batches_created(0),
          num_workers_used(0),
          analysis_time_ms(0.0),
          batching_time_ms(0.0),
          processing_time_ms(0.0),
          aggregation_time_ms(0.0),
          total_time_ms(0.0),
          hits_per_second(0.0),
          neutron_efficiency(0.0) {}
  };

  /**
   * @brief Constructor with configuration
   * @param config Temporal processing configuration
   */
  explicit TemporalGraphClusteringProcessor(
      const TemporalGraphConfig& config = TemporalGraphConfig());

  /**
   * @brief Destructor
   */
  ~TemporalGraphClusteringProcessor();

  /**
   * @brief Process hits using temporal batching for high performance
   * @param hits Input hit vector (will not be modified)
   * @return Vector of neutron events
   */
  std::vector<TDCNeutron> processHits(const std::vector<TDCHit>& hits);

  /**
   * @brief Get processing statistics from last operation
   * @return Detailed processing statistics
   */
  const ProcessingStats& getStatistics() const;

  /**
   * @brief Update processor configuration
   * @param config New temporal processing configuration
   */
  void updateConfig(const TemporalGraphConfig& config);

  /**
   * @brief Get current configuration
   * @return Current temporal processing configuration
   */
  const TemporalGraphConfig& getConfig() const;

  /**
   * @brief Reset processor state and statistics
   */
  void reset();

 private:
  TemporalGraphConfig config_;  ///< Current configuration
  ProcessingStats stats_;       ///< Processing statistics

  // Worker pool management
  std::vector<std::unique_ptr<GraphClustering>> workers_;  ///< Worker instances
  std::vector<std::unique_ptr<CentroidPeakFitting>>
      peak_fitters_;  ///< Peak fitting instances
  std::vector<std::vector<TDCNeutron>> worker_results_;  ///< Per-worker results

  /**
   * @brief Initialize worker pool
   */
  void initializeWorkers();

  /**
   * @brief Process batches in parallel using worker pool
   * @param batches Temporal batches to process
   * @return Combined results from all workers
   */
  std::vector<TDCNeutron> processBatchesParallel(
      const std::vector<HitBatch>& batches);

  /**
   * @brief Combine results from all workers with deduplication
   * @param worker_results Results from each worker
   * @return Combined and deduplicated neutron vector
   */
  std::vector<TDCNeutron> combineWorkerResults(
      const std::vector<std::vector<TDCNeutron>>& worker_results);

  /**
   * @brief Detect and remove duplicate neutrons from overlap regions
   * @param neutrons Neutron vector to deduplicate
   * @return Deduplicated neutron vector
   */
  std::vector<TDCNeutron> deduplicateNeutrons(
      std::vector<TDCNeutron>& neutrons);

  /**
   * @brief Calculate optimal number of workers for current system
   * @return Optimal worker count
   */
  size_t calculateOptimalWorkerCount() const;
};

}  // namespace tdcsophiread