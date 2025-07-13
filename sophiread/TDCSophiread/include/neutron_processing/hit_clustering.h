// TDCSophiread Hit Clustering Interface
// Clean interface for hit clustering algorithms with iterator support

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

/**
 * @brief Abstract interface for hit clustering algorithms
 *
 * Defines the contract for spatial-temporal clustering algorithms that group
 * TDCHit objects into clusters. The new interface supports iterator-based
 * processing for zero-copy parallel operation.
 *
 * Key improvements over legacy interface:
 * - Iterator-based processing for zero-copy operation
 * - Configuration management at instantiation
 * - Proper state management with reset() functionality
 * - Clear separation of concerns (clustering only, no peak fitting)
 */
class IHitClustering {
 public:
  virtual ~IHitClustering() = default;

  /**
   * @brief Configure the clustering algorithm
   * @param config Hit clustering configuration parameters
   */
  virtual void configure(const HitClusteringConfig& config) = 0;

  /**
   * @brief Get current configuration
   * @return Current hit clustering configuration
   */
  virtual const HitClusteringConfig& getConfig() const = 0;

  /**
   * @brief Perform clustering analysis on hit range (zero-copy)
   *
   * Analyzes the input hit range and assigns cluster labels. This is the main
   * clustering computation phase that implements spatial-temporal grouping.
   * Uses iterators for zero-copy processing in parallel environments.
   *
   * @param begin Iterator to first hit in range
   * @param end Iterator to one past last hit in range
   * @return Number of clusters found
   *
   * @note Hits are not modified - cluster labels stored separately
   * @note Call getClusterLabels() to access the cluster assignments
   */
  virtual size_t cluster(std::vector<TDCHit>::const_iterator begin,
                         std::vector<TDCHit>::const_iterator end) = 0;

  /**
   * @brief Reset algorithm state for new clustering run
   *
   * Clears all internal data structures and state while preserving
   * the current configuration. This allows the same algorithm instance
   * to be reused for multiple clustering operations.
   */
  virtual void reset() = 0;

  /**
   * @brief Get cluster labels assigned to hits
   * @return Vector of cluster labels (same size as last processed range)
   *
   * @note The returned vector corresponds to the hits processed in the
   *       last call to cluster(). Index i contains the cluster label
   *       for the i-th hit in the processed range.
   */
  virtual const std::vector<int>& getClusterLabels() const = 0;

  /**
   * @brief Get algorithm name for identification
   * @return Algorithm name string (e.g., "abs", "graph", "dbscan")
   */
  virtual std::string getName() const = 0;

  /**
   * @brief Get number of hits processed in last clustering run
   * @return Hit count from last clustering operation
   */
  virtual size_t getLastHitCount() const = 0;

  /**
   * @brief Get detailed performance statistics
   * @return Performance metrics from last clustering operation
   */
  virtual ClusteringStatistics getStatistics() const = 0;
};

/**
 * @brief Performance and quality statistics for clustering operations
 */
/**
 * @brief Temporal batch information for statistical analysis
 */
struct BatchStatistics {
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
  BatchStatistics()
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

  /**
   * @brief Get iterator to first hit in batch
   */
  std::vector<TDCHit>::const_iterator begin() const {
    return hits_ptr->begin() + start_index;
  }

  /**
   * @brief Get iterator to one past last hit in batch
   */
  std::vector<TDCHit>::const_iterator end() const {
    return hits_ptr->begin() + end_index;
  }
};

/**
 * @brief Utility functions for temporal batching analysis
 *
 * These functions analyze hit distributions to determine optimal
 * batch sizes for parallel temporal processing.
 */
namespace TemporalBatching {

/**
 * @brief Analyze hit distribution for temporal batching
 * @param begin Iterator to first hit
 * @param end Iterator to one past last hit
 * @param num_pulses Number of pulses to analyze (default: 2)
 * @param correlation_window Correlation window in nanoseconds (default: 75.0)
 * @return Statistical information for batch creation
 */
BatchStatistics analyzeHitDistribution(
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end, int num_pulses = 2,
    double correlation_window = 75.0);

/**
 * @brief Create temporal batches from hit range
 * @param hits_ptr Pointer to original hits vector (for zero-copy batches)
 * @param begin Iterator to first hit in range
 * @param end Iterator to one past last hit in range
 * @param stats Statistical information from analysis
 * @return Vector of temporal batches for parallel processing
 */
std::vector<HitBatch> createStatisticalBatches(
    const std::vector<TDCHit>* hits_ptr,
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end, const BatchStatistics& stats);

/**
 * @brief Create fixed-size batches (simpler alternative)
 * @param hits_ptr Pointer to original hits vector (for zero-copy batches)
 * @param begin Iterator to first hit in range
 * @param end Iterator to one past last hit in range
 * @param batch_size Target size for each batch
 * @param overlap_size Overlap between batches in number of hits
 * @return Vector of temporal batches
 */
std::vector<HitBatch> createFixedSizeBatches(
    const std::vector<TDCHit>* hits_ptr,
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end, size_t batch_size,
    size_t overlap_size);

}  // namespace TemporalBatching

}  // namespace tdcsophiread