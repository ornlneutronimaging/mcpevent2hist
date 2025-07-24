// TDCSophiread Hit Clustering Interface
// Clean interface for hit clustering algorithms with iterator support

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "neutron_processing/clustering_state.h"
#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"

namespace tdcsophiread {

/**
 * @brief Abstract interface for hit clustering algorithms (STATELESS)
 *
 * Defines the contract for spatial-temporal clustering algorithms that group
 * TDCHit objects into clusters. This interface is designed for stateless
 * operation to enable safe parallel execution with TBB.
 *
 * Key features:
 * - Stateless operation - ALL state passed via parameters
 * - Iterator-based processing for zero-copy operation
 * - Thread-safe by design - no shared mutable state
 * - Required for achieving >120M hits/sec throughput
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
   * @brief Create algorithm-specific state object
   * @return New state object for this algorithm
   *
   * Creates a new state object appropriate for this clustering algorithm.
   * The state object will be owned by the HitBatch and passed back to
   * the algorithm during clustering.
   */
  virtual std::unique_ptr<IClusteringState> createState() const = 0;

  /**
   * @brief Perform clustering analysis on hit range (stateless)
   *
   * Analyzes the input hit range and assigns cluster labels. This is a
   * stateless operation - all working data is stored in the provided state
   * object, enabling safe concurrent execution.
   *
   * @param begin Iterator to first hit in range
   * @param end Iterator to one past last hit in range
   * @param state Algorithm-specific state object
   * @param[out] cluster_labels Output vector for cluster assignments (must be
   * pre-sized)
   * @return Number of clusters found
   *
   * @note This method is const - it does not modify the algorithm instance
   * @note cluster_labels must be pre-sized to match the input range
   * @note The state object is modified during clustering
   */
  virtual size_t cluster(std::vector<TDCHit>::const_iterator begin,
                         std::vector<TDCHit>::const_iterator end,
                         IClusteringState& state,
                         std::vector<int>& cluster_labels) const = 0;

  /**
   * @brief Get algorithm name for identification
   * @return Algorithm name string (e.g., "abs", "graph", "dbscan")
   */
  virtual std::string getName() const = 0;

  /**
   * @brief Get statistics from a clustering state
   * @param state The state object after clustering
   * @param num_hits Number of hits that were processed
   * @return Performance metrics from the clustering operation
   */
  virtual ClusteringStatistics getStatistics(const IClusteringState& state,
                                             size_t num_hits) const = 0;
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
 *
 * Enhanced to support stateless clustering by owning the clustering
 * state and results for this batch.
 */
struct HitBatch {
  // Input data (zero-copy reference)
  const std::vector<TDCHit>*
      hits_ptr;               ///< Reference to original hits vector (zero-copy)
  size_t start_index;         ///< Batch start index (inclusive)
  size_t end_index;           ///< Batch end index (exclusive)
  size_t overlap_start;       ///< Overlap region start index
  size_t overlap_end;         ///< Overlap region end index
  uint32_t tof_window_start;  ///< TOF range start for this batch
  uint32_t tof_window_end;    ///< TOF range end for this batch

  // Clustering state and results
  std::unique_ptr<IClusteringState>
      clustering_state;             ///< Algorithm-specific working state
  std::vector<int> cluster_labels;  ///< Clustering results (size = end - start)

  // Neutron extraction results
  std::vector<TDCNeutron>
      neutron_results;    ///< Extracted neutrons from this batch
  int cluster_id_offset;  ///< Offset for cluster IDs (for unique IDs across
                          ///< batches)

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
        tof_window_end(0),
        cluster_id_offset(0) {}

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

  /**
   * @brief Get iterator to first hit in overlap region (includes main batch)
   */
  std::vector<TDCHit>::const_iterator overlapBegin() const {
    return hits_ptr->begin() + overlap_start;
  }

  /**
   * @brief Get iterator to one past last hit in overlap region
   */
  std::vector<TDCHit>::const_iterator overlapEnd() const {
    return hits_ptr->begin() + overlap_end;
  }

  /**
   * @brief Initialize cluster labels vector for this batch
   */
  void initializeResults() {
    cluster_labels.clear();
    // Resize for the main batch range only
    cluster_labels.resize(size(), -1);  // Initialize all to unclustered
  }

  /**
   * @brief Get the size of the overlap region
   */
  size_t overlapSize() const { return overlap_end - overlap_start; }
};

/**
 * @brief Utility functions for temporal batching analysis
 *
 * These functions analyze hit distributions to determine optimal
 * batch sizes for parallel temporal processing.
 */
namespace TemporalBatching {

// Statistical analysis functions removed - using simple fixed batching instead

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