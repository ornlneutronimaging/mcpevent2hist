// TDCSophiread Neutron Extraction Interface
// Clean interface for extracting neutron properties from clustered hits

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "neutron_processing/neutron_config.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"

namespace tdcsophiread {

/**
 * @brief Abstract interface for neutron extraction algorithms
 *
 * Defines the contract for algorithms that extract neutron properties
 * (position, amplitude, timing) from clusters of hits. This was previously
 * called "peak fitting" but "neutron extraction" better describes the purpose.
 *
 * Key improvements over legacy interface:
 * - Clear separation from clustering (takes cluster labels as input)
 * - Iterator-based processing for zero-copy operation
 * - Configuration management at instantiation
 * - Proper state management with reset() functionality
 */
class INeutronExtraction {
 public:
  virtual ~INeutronExtraction() = default;

  /**
   * @brief Configure the neutron extraction algorithm
   * @param config Neutron extraction configuration parameters
   */
  virtual void configure(const NeutronExtractionConfig& config) = 0;

  /**
   * @brief Get current configuration
   * @return Current neutron extraction configuration
   */
  virtual const NeutronExtractionConfig& getConfig() const = 0;

  /**
   * @brief Extract neutron events from clustered hits
   *
   * Processes hits with cluster labels to determine neutron properties.
   * Each cluster is analyzed to produce a single TDCNeutron event with
   * precise coordinates and consolidated timing/amplitude information.
   *
   * @param begin Iterator to first hit in range
   * @param end Iterator to one past last hit in range
   * @param cluster_labels Cluster labels for each hit in the range
   * @return Vector of neutron events (one per valid cluster)
   *
   * @note cluster_labels.size() must equal std::distance(begin, end)
   * @note Hits with cluster_id < 0 are considered noise and ignored
   */
  virtual std::vector<TDCNeutron> extract(
      std::vector<TDCHit>::const_iterator begin,
      std::vector<TDCHit>::const_iterator end,
      const std::vector<int>& cluster_labels) = 0;

  /**
   * @brief Reset algorithm state for new extraction run
   *
   * Clears all internal data structures and state while preserving
   * the current configuration. This allows the same algorithm instance
   * to be reused for multiple extraction operations.
   */
  virtual void reset() = 0;

  /**
   * @brief Get algorithm name for identification
   * @return Algorithm name string (e.g., "centroid", "gaussian", "ml")
   */
  virtual std::string getName() const = 0;

  /**
   * @brief Get super-resolution factor applied
   * @return Coordinate scaling factor (e.g., 8 for 8x super-resolution)
   */
  virtual double getSuperResolutionFactor() const = 0;

  /**
   * @brief Get detailed performance statistics
   * @return Performance metrics from last extraction operation
   */
  virtual ExtractionStatistics getStatistics() const = 0;
};

/**
 * @brief Performance and quality statistics for neutron extraction operations
 */
/**
 * @brief Cluster information for extraction algorithms
 *
 * Provides convenient access to hits belonging to a single cluster
 */
struct ClusterInfo {
  int cluster_id;  ///< Cluster identifier
  std::vector<const TDCHit*>
      hits;  ///< Pointers to hits in this cluster (no copying)

  // Precomputed properties for efficiency
  uint16_t min_x, max_x;      ///< Bounding box X range
  uint16_t min_y, max_y;      ///< Bounding box Y range
  uint32_t min_tof, max_tof;  ///< TOF range
  uint16_t total_tot;         ///< Sum of all TOT values

  /**
   * @brief Default constructor
   */
  ClusterInfo()
      : cluster_id(-1),
        min_x(0),
        max_x(0),
        min_y(0),
        max_y(0),
        min_tof(0),
        max_tof(0),
        total_tot(0) {}

  /**
   * @brief Get cluster size (number of hits)
   */
  size_t size() const { return hits.size(); }

  /**
   * @brief Check if cluster is valid (has hits)
   */
  bool isValid() const { return !hits.empty() && cluster_id >= 0; }

  /**
   * @brief Calculate cluster centroid (simple average)
   * @return Centroid coordinates
   */
  std::pair<double, double> calculateCentroid() const;

  /**
   * @brief Calculate weighted centroid (TOT-weighted)
   * @return Weighted centroid coordinates
   */
  std::pair<double, double> calculateWeightedCentroid() const;

  /**
   * @brief Calculate cluster spread (RMS)
   * @return RMS spread in X and Y directions
   */
  std::pair<double, double> calculateSpread() const;
};

/**
 * @brief Utility functions for neutron extraction
 */
namespace ExtractionUtils {

/**
 * @brief Group hits by cluster ID
 * @param begin Iterator to first hit
 * @param end Iterator to one past last hit
 * @param cluster_labels Cluster labels for each hit
 * @return Vector of ClusterInfo objects, one per unique cluster
 *
 * @note Hits with cluster_id < 0 are ignored (considered noise)
 */
std::vector<ClusterInfo> groupHitsByClusters(
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end,
    const std::vector<int>& cluster_labels);

/**
 * @brief Validate cluster for neutron extraction
 * @param cluster Cluster to validate
 * @param config Extraction configuration
 * @return True if cluster meets criteria for extraction
 */
bool validateCluster(const ClusterInfo& cluster,
                     const NeutronExtractionConfig& config);

/**
 * @brief Create neutron from cluster using simple centroid
 * @param cluster Cluster information
 * @param config Extraction configuration
 * @return TDCNeutron with centroid position and consolidated properties
 */
TDCNeutron createNeutronFromCentroid(const ClusterInfo& cluster,
                                     const NeutronExtractionConfig& config);

/**
 * @brief Calculate position uncertainty estimate
 * @param cluster Cluster information
 * @return Estimated position uncertainty in pixels
 */
double estimatePositionUncertainty(const ClusterInfo& cluster);

/**
 * @brief Consolidate timing information from cluster
 * @param cluster Cluster information
 * @return Consolidated TOF value (e.g., TOT-weighted average)
 */
uint32_t consolidateClusterTiming(const ClusterInfo& cluster);

/**
 * @brief Consolidate amplitude information from cluster
 * @param cluster Cluster information
 * @return Consolidated TOT value (e.g., sum or peak)
 */
uint16_t consolidateClusterAmplitude(const ClusterInfo& cluster);

}  // namespace ExtractionUtils

}  // namespace tdcsophiread