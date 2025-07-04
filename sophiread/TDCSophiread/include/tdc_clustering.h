// TDCSophiread Clustering Interfaces
// Abstract interfaces for neutron clustering algorithms and peak fitting
// Based on FastSophiread clustering architecture with TDC-specific
// optimizations

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "tdc_hit.h"

namespace tdcsophiread {

// Forward declarations
struct TDCNeutron;
struct ClusteringConfig;

/**
 * @brief Abstract interface for clustering algorithms
 *
 * Defines the contract for spatial-temporal clustering algorithms that group
 * TDCHit objects into clusters for subsequent neutron event extraction.
 *
 * The clustering process is typically a two-pass algorithm:
 * 1. fit() - Analyzes hits and assigns cluster labels
 * 2. getClusterLabels() - Returns cluster assignments for each hit
 */
class IClusteringAlgorithm {
 public:
  virtual ~IClusteringAlgorithm() = default;

  /**
   * @brief Configure the clustering algorithm
   * @param config Clustering configuration parameters
   */
  virtual void configure(const ClusteringConfig& config) = 0;

  /**
   * @brief Perform clustering analysis on hits
   *
   * Analyzes the input hits and assigns cluster labels. This is the main
   * clustering computation phase that implements spatial-temporal grouping.
   *
   * @param hits Input hits to cluster (will be modified with cluster labels)
   * @return Number of clusters found
   */
  virtual size_t fit(std::vector<TDCHit>& hits) = 0;

  /**
   * @brief Get cluster labels assigned to hits
   * @return Vector of cluster labels (same size as input hits)
   */
  virtual const std::vector<int>& getClusterLabels() const = 0;

  /**
   * @brief Get algorithm name for identification
   * @return Algorithm name string
   */
  virtual std::string getName() const = 0;

  /**
   * @brief Reset algorithm state for new clustering run
   */
  virtual void reset() = 0;

  /**
   * @brief Get performance metrics from last clustering run
   * @return Number of hits processed
   */
  virtual size_t getLastHitCount() const = 0;
};

/**
 * @brief Abstract interface for peak fitting algorithms
 *
 * Defines the contract for algorithms that determine sub-pixel neutron
 * positions from clustered hits. Peak fitting enables super-resolution
 * coordinate calculation.
 */
class IPeakFitting {
 public:
  virtual ~IPeakFitting() = default;

  /**
   * @brief Configure the peak fitting algorithm
   * @param config Peak fitting configuration parameters
   */
  virtual void configure(const ClusteringConfig& config) = 0;

  /**
   * @brief Extract neutron events from clustered hits
   *
   * Processes hits with cluster labels to determine sub-pixel neutron
   * positions. Each cluster is analyzed to produce a single TDCNeutron event
   * with precise coordinates and consolidated timing/amplitude information.
   *
   * @param hits Input hits with cluster labels assigned
   * @return Vector of neutron events (one per cluster)
   */
  virtual std::vector<TDCNeutron> extractNeutrons(
      const std::vector<TDCHit>& hits) = 0;

  /**
   * @brief Get algorithm name for identification
   * @return Algorithm name string
   */
  virtual std::string getName() const = 0;

  /**
   * @brief Get super-resolution factor applied
   * @return Coordinate scaling factor (e.g., 8 for 8x super-resolution)
   */
  virtual double getSuperResolutionFactor() const = 0;
};

/**
 * @brief Main clustering processor interface
 *
 * Combines clustering algorithm and peak fitting to provide a complete
 * hits-to-neutrons processing pipeline. This is the primary interface
 * for neutron event extraction from raw hit data.
 */
class IClusterProcessor {
 public:
  virtual ~IClusterProcessor() = default;

  /**
   * @brief Configure the entire clustering pipeline
   * @param config Clustering and peak fitting configuration
   */
  virtual void configure(const ClusteringConfig& config) = 0;

  /**
   * @brief Process hits into neutron events
   *
   * Complete pipeline: clustering + peak fitting to convert raw hits
   * into sub-pixel precision neutron events.
   *
   * @param hits Input TDC hits
   * @return Vector of neutron events with sub-pixel coordinates
   */
  virtual std::vector<TDCNeutron> processHits(std::vector<TDCHit>& hits) = 0;

  /**
   * @brief Get clustering algorithm name
   * @return Name of clustering algorithm in use
   */
  virtual std::string getClusteringAlgorithm() const = 0;

  /**
   * @brief Get peak fitting algorithm name
   * @return Name of peak fitting algorithm in use
   */
  virtual std::string getPeakFittingAlgorithm() const = 0;

  /**
   * @brief Get performance metrics from last processing run
   * @return Processing time in milliseconds
   */
  virtual double getLastProcessingTimeMs() const = 0;

  /**
   * @brief Get hit processing rate
   * @return Hits processed per second
   */
  virtual double getLastHitsPerSecond() const = 0;

  /**
   * @brief Get neutron extraction efficiency
   * @return Ratio of neutrons to input hits
   */
  virtual double getLastNeutronEfficiency() const = 0;
};

/**
 * @brief Factory for creating clustering algorithms
 */
class ClusteringAlgorithmFactory {
 public:
  /**
   * @brief Create clustering algorithm by name
   * @param algorithm_name Name of algorithm ("abs", "dbscan", etc.)
   * @return Unique pointer to clustering algorithm
   * @throws std::invalid_argument if algorithm name is unknown
   */
  static std::unique_ptr<IClusteringAlgorithm> create(
      const std::string& algorithm_name);

  /**
   * @brief Get list of available clustering algorithms
   * @return Vector of algorithm names
   */
  static std::vector<std::string> getAvailableAlgorithms();
};

/**
 * @brief Factory for creating peak fitting algorithms
 */
class PeakFittingFactory {
 public:
  /**
   * @brief Create peak fitting algorithm by name
   * @param algorithm_name Name of algorithm ("centroid", "fastgaussian", etc.)
   * @return Unique pointer to peak fitting algorithm
   * @throws std::invalid_argument if algorithm name is unknown
   */
  static std::unique_ptr<IPeakFitting> create(
      const std::string& algorithm_name);

  /**
   * @brief Get list of available peak fitting algorithms
   * @return Vector of algorithm names
   */
  static std::vector<std::string> getAvailableAlgorithms();
};

/**
 * @brief Factory for creating complete cluster processors
 */
class ClusterProcessorFactory {
 public:
  /**
   * @brief Create cluster processor with specified algorithms
   * @param clustering_algorithm Name of clustering algorithm
   * @param peak_fitting_algorithm Name of peak fitting algorithm
   * @return Unique pointer to cluster processor
   */
  static std::unique_ptr<IClusterProcessor> create(
      const std::string& clustering_algorithm,
      const std::string& peak_fitting_algorithm);

  /**
   * @brief Create cluster processor from configuration
   * @param config Clustering configuration specifying algorithms
   * @return Unique pointer to cluster processor
   */
  static std::unique_ptr<IClusterProcessor> create(
      const ClusteringConfig& config);
};

}  // namespace tdcsophiread