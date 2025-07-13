// TDCSophiread Basic Neutron Processor Implementation
// Single-threaded processor combining ABS clustering and centroid extraction

#pragma once

#include <chrono>
#include <memory>

#include "neutron_processing/neutron_config.h"
#include "neutron_processing/neutron_processing.h"
#include "neutron_processing/simple_abs_clustering.h"
#include "neutron_processing/simple_centroid_extraction.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"

namespace tdcsophiread {

/**
 * @brief Basic single-threaded neutron processor
 *
 * Simple implementation that combines SimpleABSClustering and
 * SimpleCentroidExtraction in a single-threaded pipeline. Useful for testing,
 * debugging, and as a baseline for performance comparison with parallel
 * implementations.
 *
 * Processing pipeline:
 * 1. SimpleABSClustering: hits → cluster labels
 * 2. SimpleCentroidExtraction: hits + labels → neutrons
 */
class BasicNeutronProcessor : public INeutronProcessor {
 private:
  // Configuration
  NeutronProcessingConfig config_;

  // Algorithm instances
  std::unique_ptr<SimpleABSClustering> clusterer_;
  std::unique_ptr<SimpleCentroidExtraction> extractor_;

  // Performance tracking
  mutable ProcessingStatistics last_stats_;
  mutable std::chrono::high_resolution_clock::time_point start_time_;

  /**
   * @brief Initialize algorithm instances based on configuration
   */
  void initializeAlgorithms();

  /**
   * @brief Update processing statistics after operation
   * @param num_hits Number of input hits
   * @param num_neutrons Number of output neutrons
   * @param total_time_ms Total processing time
   */
  void updateStatistics(size_t num_hits, size_t num_neutrons,
                        double total_time_ms);

 public:
  /**
   * @brief Default constructor with VENUS defaults
   */
  BasicNeutronProcessor();

  /**
   * @brief Constructor with specific configuration
   */
  explicit BasicNeutronProcessor(const NeutronProcessingConfig& config);

  // INeutronProcessor interface implementation
  std::vector<TDCNeutron> processHits(
      std::vector<TDCHit>::const_iterator begin,
      std::vector<TDCHit>::const_iterator end) override;

  NeutronProcessingResults processHitsWithLabels(
      std::vector<TDCHit>::const_iterator begin,
      std::vector<TDCHit>::const_iterator end) override;

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
   * @brief Get direct access to clustering algorithm (for testing)
   * @return Pointer to clustering algorithm instance
   */
  SimpleABSClustering* getClusterer() const { return clusterer_.get(); }

  /**
   * @brief Get direct access to extraction algorithm (for testing)
   * @return Pointer to extraction algorithm instance
   */
  SimpleCentroidExtraction* getExtractor() const { return extractor_.get(); }
};

}  // namespace tdcsophiread