// TDCSophiread Stateless Hit Clustering Interface
// Interface for thread-safe stateless clustering algorithms

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "neutron_processing/clustering_state.h"
#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"

namespace tdcsophiread {

/**
 * @brief Abstract interface for stateless hit clustering algorithms
 *
 * Defines the contract for spatial-temporal clustering algorithms that operate
 * in a stateless manner. All working state is passed via parameters, enabling
 * safe concurrent execution in parallel processing environments.
 *
 * Key features:
 * - Stateless operation - all state passed via parameters
 * - Iterator-based processing for zero-copy operation
 * - Thread-safe concurrent execution
 * - Configuration management at instantiation
 */
class IStatelessHitClustering {
 public:
  virtual ~IStatelessHitClustering() = default;

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
   * @return Algorithm name string (e.g., "stateless_abs", "stateless_graph")
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

}  // namespace tdcsophiread