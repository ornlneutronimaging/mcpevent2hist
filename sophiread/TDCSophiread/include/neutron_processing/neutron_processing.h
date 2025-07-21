// TDCSophiread Neutron Processing Interface
// Main interface for the complete hits→neutrons pipeline with temporal batching

#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "neutron_processing/hit_clustering.h"
#include "neutron_processing/neutron_config.h"
#include "neutron_processing/neutron_extraction.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"

namespace tdcsophiread {

/**
 * @brief Results from neutron processing operations
 *
 * Contains neutrons and optionally cluster labels for diagnostics.
 * Production workflows only need the neutrons.
 */
struct NeutronProcessingResults {
  std::vector<TDCNeutron> neutrons;  ///< Extracted neutron events
  std::vector<int>
      cluster_labels;       ///< Cluster labels (optional, for diagnostics)
  bool has_cluster_labels;  ///< Whether cluster labels are included

  /**
   * @brief Constructor for production results (neutrons only)
   */
  explicit NeutronProcessingResults(std::vector<TDCNeutron>&& neutrons_)
      : neutrons(std::move(neutrons_)), has_cluster_labels(false) {}

  /**
   * @brief Constructor for diagnostic results (neutrons + cluster labels)
   */
  NeutronProcessingResults(std::vector<TDCNeutron>&& neutrons_,
                           std::vector<int>&& cluster_labels_)
      : neutrons(std::move(neutrons_)),
        cluster_labels(std::move(cluster_labels_)),
        has_cluster_labels(true) {}
};

/**
 * @brief Abstract interface for neutron processing algorithms
 *
 * Defines the contract for complete hits-to-neutrons processing pipelines.
 * Implementations combine hit clustering and neutron extraction through
 * various strategies (single-threaded, parallel temporal batching, etc.).
 *
 * Key design principles:
 * - Iterator-based processing for zero-copy operation
 * - Configuration at instantiation for thread safety
 * - Clean separation between clustering and extraction phases
 * - Support for different parallelization strategies
 * - Optional cluster label tracking for detector diagnostics
 */
class INeutronProcessor {
 public:
  virtual ~INeutronProcessor() = default;

  /**
   * @brief Process hits to extract neutron events (production path)
   *
   * Main processing interface that converts raw TDC hits into neutron
   * events with sub-pixel precision. Optimized for production use with
   * true zero-copy operation.
   *
   * @param hits Reference to hits vector (enables zero-copy batching)
   * @param start_offset Starting offset in hits vector (default: 0)
   * @param end_offset Ending offset in hits vector (default: SIZE_MAX = entire
   * vector)
   * @return Vector of neutron events
   *
   * @note Input hits are in acquisition order and MUST NOT be sorted
   * @note Implementation should leverage TOF periodicity for batching
   * @note Zero-copy design: no data copying, works directly with input vector
   */
  virtual std::vector<TDCNeutron> processHits(const std::vector<TDCHit>& hits,
                                              size_t start_offset = 0,
                                              size_t end_offset = SIZE_MAX) = 0;

  /**
   * @brief Process hits with cluster label tracking (diagnostics path)
   *
   * Extended processing interface that also returns cluster labels
   * for detector diagnostics and troubleshooting workflows.
   *
   * @param hits Reference to hits vector (enables zero-copy batching)
   * @param start_offset Starting offset in hits vector (default: 0)
   * @param end_offset Ending offset in hits vector (default: SIZE_MAX = entire
   * vector)
   * @return Results containing both neutrons and cluster labels
   *
   * @note This may have slightly higher memory usage than processHits()
   * @note Cluster labels are indexed to match the input hit range
   * @note Zero-copy design: no data copying, works directly with input vector
   */
  virtual NeutronProcessingResults processHitsWithLabels(
      const std::vector<TDCHit>& hits, size_t start_offset = 0,
      size_t end_offset = SIZE_MAX) = 0;

  /**
   * @brief Configure the processor
   * @param config Complete neutron processing configuration
   */
  virtual void configure(const NeutronProcessingConfig& config) = 0;

  /**
   * @brief Get current configuration
   * @return Current neutron processing configuration
   */
  virtual const NeutronProcessingConfig& getConfig() const = 0;

  /**
   * @brief Get hit clustering algorithm name
   * @return Name of the clustering algorithm in use
   */
  virtual std::string getHitClusteringAlgorithm() const = 0;

  /**
   * @brief Get neutron extraction algorithm name
   * @return Name of the extraction algorithm in use
   */
  virtual std::string getNeutronExtractionAlgorithm() const = 0;

  /**
   * @brief Get last processing time
   * @return Processing time for last operation in milliseconds
   */
  virtual double getLastProcessingTimeMs() const = 0;

  /**
   * @brief Get last processing rate
   * @return Hits processed per second in last operation
   */
  virtual double getLastHitsPerSecond() const = 0;

  /**
   * @brief Get last neutron efficiency
   * @return Ratio of neutrons to hits from last operation
   */
  virtual double getLastNeutronEfficiency() const = 0;

  /**
   * @brief Reset processor state
   *
   * Clears internal state and statistics while preserving configuration.
   * Useful for processing multiple files with the same settings.
   */
  virtual void reset() = 0;

  /**
   * @brief Get detailed processing statistics
   * @return Comprehensive statistics from last processing operation
   */
  virtual ProcessingStatistics getStatistics() const = 0;
};

/**
 * @brief Comprehensive statistics for neutron processing operations
 */
/**
 * @brief Temporal batching neutron processor with parallel execution
 *
 * Advanced implementation that uses statistical temporal analysis to create
 * optimal batches for parallel processing. Each worker thread has its own
 * algorithm instances for true parallel execution without synchronization.
 *
 * Processing phases:
 * 1. Analyze hit distribution to determine optimal batch boundaries
 * 2. Create temporal batches with appropriate overlaps and cluster ID offsets
 * 3. Process batches in parallel using worker pool
 * 4. Aggregate results with deduplication in overlap regions
 * 5. Optionally map cluster labels back to hits for diagnostics
 */
class TemporalNeutronProcessor : public INeutronProcessor {
 private:
  // Configuration
  NeutronProcessingConfig config_;

  // Pre-allocated algorithm pool - STATELESS
  // Each thread gets dedicated algorithm instances
  struct AlgorithmSet {
    std::unique_ptr<IHitClustering>
        clusterer;  ///< Stateless clustering algorithm
    std::unique_ptr<INeutronExtraction>
        extractor;  ///< Stateless extraction algorithm
  };
  std::vector<AlgorithmSet> algorithm_pool_;

  // Performance tracking
  mutable ProcessingStatistics last_stats_;

  /**
   * @brief Initialize algorithm pool based on configuration
   */
  void initializeAlgorithmPool();

  /**
   * @brief Process batches in parallel using TBB
   */
  void processBatchesParallel(std::vector<HitBatch>& batches);

  /**
   * @brief Process a single batch using specified algorithms
   */
  void processSingleBatch(HitBatch& batch, const AlgorithmSet& algorithms);

  /**
   * @brief Collect neutron results from all batches
   */
  std::vector<TDCNeutron> collectNeutronResults(
      const std::vector<HitBatch>& batches);

  /**
   * @brief Remove duplicate neutrons from overlap regions
   */
  std::vector<TDCNeutron> deduplicateNeutrons(std::vector<TDCNeutron> neutrons);

  /**
   * @brief Calculate cluster ID offsets for batches
   */
  void calculateClusterIdOffsets(std::vector<HitBatch>& batches);

  /**
   * @brief Update performance statistics after processing
   */
  void updateStatistics(size_t hits_processed, size_t neutrons_found,
                        double processing_time_ms, size_t num_batches);

 public:
  /**
   * @brief Default constructor with VENUS defaults
   */
  TemporalNeutronProcessor();

  /**
   * @brief Destructor
   */
  ~TemporalNeutronProcessor() override;

  /**
   * @brief Constructor with specific configuration
   */
  explicit TemporalNeutronProcessor(const NeutronProcessingConfig& config);

  // INeutronProcessor interface implementation
  std::vector<TDCNeutron> processHits(const std::vector<TDCHit>& hits,
                                      size_t start_offset = 0,
                                      size_t end_offset = SIZE_MAX) override;

  NeutronProcessingResults processHitsWithLabels(
      const std::vector<TDCHit>& hits, size_t start_offset = 0,
      size_t end_offset = SIZE_MAX) override;

  void configure(const NeutronProcessingConfig& config) override;
  const NeutronProcessingConfig& getConfig() const override { return config_; }
  std::string getHitClusteringAlgorithm() const override;
  std::string getNeutronExtractionAlgorithm() const override;
  double getLastProcessingTimeMs() const override;
  double getLastHitsPerSecond() const override;
  double getLastNeutronEfficiency() const override;
  void reset() override;
  ProcessingStatistics getStatistics() const override { return last_stats_; }

  /**
   * @brief Get number of workers in pool
   */
  size_t getNumWorkers() const { return algorithm_pool_.size(); }
};

/**
 * @brief Utility functions for neutron processing
 *
 * Common functionality used by processor implementations.
 * All functions respect the natural acquisition order of hits.
 */
namespace ProcessingUtils {

/**
 * @brief Validate input hits for processing
 * @param begin Iterator to first hit
 * @param end Iterator to one past last hit
 * @return True if hits are valid for processing
 * @throws std::invalid_argument with specific error message
 */
bool validateHits(std::vector<TDCHit>::const_iterator begin,
                  std::vector<TDCHit>::const_iterator end);

/**
 * @brief Calculate processing efficiency metrics
 * @param num_hits Number of input hits
 * @param num_neutrons Number of output neutrons
 * @param num_clusters Number of clusters found
 * @return Tuple of (neutron_efficiency, mean_cluster_size)
 */
std::tuple<double, double> calculateEfficiencyMetrics(size_t num_hits,
                                                      size_t num_neutrons,
                                                      size_t num_clusters);

/**
 * @brief Estimate memory requirements for processing
 * @param num_hits Number of hits to process
 * @param config Processing configuration
 * @param num_workers Number of worker threads
 * @return Estimated peak memory usage in MB
 */
double estimateMemoryUsage(size_t num_hits,
                           const NeutronProcessingConfig& config,
                           size_t num_workers);

/**
 * @brief Check if two neutrons are potential duplicates
 *
 * Used for deduplication in overlap regions. Neutrons are considered
 * duplicates if they are spatially close and have similar timing.
 *
 * @param n1 First neutron
 * @param n2 Second neutron
 * @param spatial_tolerance Spatial distance tolerance in pixels
 * @param tof_tolerance TOF difference tolerance in 25ns units
 * @return True if neutrons are likely duplicates
 */
bool areNeutronsDuplicate(const TDCNeutron& n1, const TDCNeutron& n2,
                          double spatial_tolerance = 1.0,
                          uint32_t tof_tolerance = 10);

/**
 * @brief Format processing statistics as human-readable string
 * @param stats Processing statistics
 * @return Formatted string with key metrics
 */
std::string formatStatistics(const ProcessingStatistics& stats);

/**
 * @brief Map cluster labels back to hits for diagnostics
 *
 * Takes cluster labels from multiple temporal batches and assigns them
 * back to the original hits with proper offset handling and overlap resolution.
 *
 * @param hits_begin Iterator to first hit in original range
 * @param hits_end Iterator to one past last hit in original range
 * @param batches Vector of temporal batches that were processed
 * @param batch_cluster_labels Cluster labels for each batch
 * @param output_hits Output vector to populate with cluster IDs
 *
 * @note output_hits will be resized to match input range
 * @note For overlapping regions, later batches overwrite earlier ones
 */
void mapClusterLabelsToHits(
    std::vector<TDCHit>::const_iterator hits_begin,
    std::vector<TDCHit>::const_iterator hits_end,
    const std::vector<HitBatch>& batches,
    const std::vector<std::vector<int>>& batch_cluster_labels,
    std::vector<TDCHit>& output_hits);

/**
 * @brief Calculate optimal cluster ID offsets for temporal batches
 *
 * Analyzes expected cluster counts and assigns non-overlapping cluster ID
 * ranges to each batch to avoid conflicts during parallel processing.
 *
 * @param batches Vector of temporal batches
 * @param estimated_clusters_per_batch Expected clusters per batch
 * @return Vector of cluster ID offsets for each batch
 */
std::vector<int> calculateClusterIdOffsets(
    const std::vector<HitBatch>& batches,
    size_t estimated_clusters_per_batch = 1000);

}  // namespace ProcessingUtils

}  // namespace tdcsophiread