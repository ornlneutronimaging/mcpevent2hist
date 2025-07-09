// TDCSophiread Cluster Processor
// Main clustering pipeline implementing hits-to-neutrons conversion

#pragma once

#include <chrono>
#include <memory>
#include <vector>

#include "tdc_clustering.h"
#include "tdc_clustering_config.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"

namespace tdcsophiread {

// Forward declarations
class TemporalGraphClusteringProcessor;

/**
 * @brief Main cluster processor implementing the complete hits-to-neutrons
 * pipeline
 *
 * Combines clustering algorithm and peak fitting to convert raw TDC hits into
 * neutron events with sub-pixel precision. Provides performance monitoring
 * and configurable algorithm selection.
 *
 * Processing Pipeline:
 * 1. Input: Vector of TDCHit objects from TPX3 processing
 * 2. Clustering: Group hits spatially and temporally (e.g., ABS algorithm)
 * 3. Peak Fitting: Calculate sub-pixel neutron positions (e.g., centroid)
 * 4. Output: Vector of TDCNeutron events with sub-pixel coordinates
 */
class TDCClusterProcessor : public IClusterProcessor {
 private:
  std::unique_ptr<IClusteringAlgorithm>
      clustering_algorithm_;  ///< Clustering algorithm instance
  std::unique_ptr<IPeakFitting>
      peak_fitting_algorithm_;  ///< Peak fitting algorithm instance
  std::unique_ptr<TemporalGraphClusteringProcessor>
      temporal_processor_;   ///< Temporal graph clustering processor
  ClusteringConfig config_;  ///< Current configuration

  // Performance metrics
  mutable std::chrono::high_resolution_clock::time_point start_time_;
  mutable std::chrono::high_resolution_clock::time_point end_time_;
  size_t last_hit_count_;           ///< Hits processed in last run
  size_t last_neutron_count_;       ///< Neutrons extracted in last run
  double last_processing_time_ms_;  ///< Processing time for last run

  /**
   * @brief Initialize algorithms based on configuration
   */
  void initializeAlgorithms();

  /**
   * @brief Update performance metrics after processing
   * @param hit_count Number of hits processed
   * @param neutron_count Number of neutrons extracted
   */
  void updatePerformanceMetrics(size_t hit_count, size_t neutron_count);

 public:
  /**
   * @brief Default constructor with VENUS defaults
   */
  TDCClusterProcessor();

  /**
   * @brief Constructor with specific configuration
   * @param config Clustering configuration
   */
  explicit TDCClusterProcessor(const ClusteringConfig& config);

  /**
   * @brief Destructor
   */
  ~TDCClusterProcessor() override;

  // IClusterProcessor interface implementation
  void configure(const ClusteringConfig& config) override;
  std::vector<TDCNeutron> processHits(std::vector<TDCHit>& hits) override;
  std::string getClusteringAlgorithm() const override;
  std::string getPeakFittingAlgorithm() const override;
  double getLastProcessingTimeMs() const override;
  double getLastHitsPerSecond() const override;
  double getLastNeutronEfficiency() const override;

  /**
   * @brief Process hits with progress callback
   * @param hits Input hits to cluster
   * @param progress_callback Optional callback for progress updates
   * @return Vector of neutron events
   */
  std::vector<TDCNeutron> processHitsWithProgress(
      std::vector<TDCHit>& hits,
      const std::function<void(double)>& progress_callback = nullptr);

  /**
   * @brief Get current configuration
   * @return Reference to current clustering configuration
   */
  const ClusteringConfig& getConfiguration() const;

  /**
   * @brief Get clustering algorithm instance
   * @return Pointer to clustering algorithm (for advanced access)
   */
  IClusteringAlgorithm* getClusteringAlgorithmPtr() const;

  /**
   * @brief Get peak fitting algorithm instance
   * @return Pointer to peak fitting algorithm (for advanced access)
   */
  IPeakFitting* getPeakFittingAlgorithmPtr() const;

  /**
   * @brief Get last neutron count
   * @return Number of neutrons extracted in last processing run
   */
  size_t getLastNeutronCount() const;

  /**
   * @brief Get clustering statistics from last run
   * @return Neutron statistics including cluster size distribution
   */
  NeutronStatistics getLastStatistics() const;

  /**
   * @brief Validate input hits for clustering
   * @param hits Input hits to validate
   * @return True if hits are valid for clustering
   * @throws std::invalid_argument if hits contain invalid data
   */
  bool validateInputHits(const std::vector<TDCHit>& hits) const;

  /**
   * @brief Reset processor state and clear metrics
   */
  void reset();

  /**
   * @brief Check if clustering is enabled in configuration
   * @return True if clustering is enabled
   */
  bool isClusteringEnabled() const;

  /**
   * @brief Create processing summary string
   * @return Human-readable summary of last processing run
   */
  std::string getProcessingSummary() const;
};

/**
 * @brief Utility functions for cluster processing
 */
class ClusterProcessingUtils {
 public:
  /**
   * @brief Filter hits by chip before clustering
   * @param hits Input hits
   * @param chip_id Target chip ID
   * @return Hits from specified chip only
   */
  static std::vector<TDCHit> filterHitsByChip(const std::vector<TDCHit>& hits,
                                              uint8_t chip_id);

  /**
   * @brief Sort hits by timestamp for temporal clustering
   * @param hits Hits to sort (modified in-place)
   */
  static void sortHitsByTimestamp(std::vector<TDCHit>& hits);

  /**
   * @brief Remove hits with invalid cluster labels
   * @param hits Input hits with cluster labels
   * @return Hits with valid cluster assignments (cluster_id >= 0)
   */
  static std::vector<TDCHit> filterValidClusteredHits(
      const std::vector<TDCHit>& hits);

  /**
   * @brief Calculate cluster statistics for labeled hits
   * @param hits Hits with cluster labels assigned
   * @return Map of cluster_id to hit count
   */
  static std::map<int, size_t> calculateClusterSizes(
      const std::vector<TDCHit>& hits);

  /**
   * @brief Validate cluster labels consistency
   * @param hits Hits with cluster labels
   * @return True if cluster labels are consistent
   */
  static bool validateClusterLabels(const std::vector<TDCHit>& hits);

  /**
   * @brief Create hit subset for algorithm testing
   * @param hits Input hits
   * @param max_hits Maximum hits to include
   * @param random_sample Use random sampling (vs first N hits)
   * @return Subset of hits for testing
   */
  static std::vector<TDCHit> createHitSubset(const std::vector<TDCHit>& hits,
                                             size_t max_hits,
                                             bool random_sample = false);

  /**
   * @brief Estimate memory usage for clustering
   * @param hit_count Number of hits to process
   * @param config Clustering configuration
   * @return Estimated memory usage in bytes
   */
  static size_t estimateMemoryUsage(size_t hit_count,
                                    const ClusteringConfig& config);

  /**
   * @brief Get recommended chunk size for memory-constrained processing
   * @param total_hits Total number of hits
   * @param available_memory_mb Available memory in MB
   * @param config Clustering configuration
   * @return Recommended chunk size
   */
  static size_t getRecommendedChunkSize(size_t total_hits,
                                        size_t available_memory_mb,
                                        const ClusteringConfig& config);
};

}  // namespace tdcsophiread