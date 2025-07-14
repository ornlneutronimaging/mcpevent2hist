// TDCSophiread Basic Neutron Processor Implementation
// Single-threaded processor combining ABS clustering and centroid extraction

#include "neutron_processing/basic_neutron_processor.h"

#include <stdexcept>

#include "neutron_processing/neutron_factories.h"

namespace tdcsophiread {

BasicNeutronProcessor::BasicNeutronProcessor()
    : config_(NeutronProcessingConfig::venusDefaults()), last_stats_() {
  initializeAlgorithms();
}

BasicNeutronProcessor::BasicNeutronProcessor(
    const NeutronProcessingConfig& config)
    : config_(config), last_stats_() {
  config_.validate();
  initializeAlgorithms();
}

void BasicNeutronProcessor::initializeAlgorithms() {
  // Create clustering algorithm instance using factory
  clusterer_ = HitClusteringFactory::create(config_.clustering.algorithm,
                                            config_.clustering);

  // Create extraction algorithm instance using factory
  extractor_ = NeutronExtractionFactory::create(config_.extraction.algorithm,
                                                config_.extraction);
}

std::vector<TDCNeutron> BasicNeutronProcessor::processHits(
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end) {
  start_time_ = std::chrono::high_resolution_clock::now();

  const size_t num_hits = std::distance(begin, end);

  if (num_hits == 0) {
    updateStatistics(0, 0, 0.0);
    return {};
  }

  // Create temporary copy for clustering
  std::vector<TDCHit> mutable_hits(begin, end);

  clusterer_->cluster(mutable_hits.begin(), mutable_hits.end());
  const std::vector<int>& cluster_labels = clusterer_->getClusterLabels();

  std::vector<TDCNeutron> neutrons =
      extractor_->extract(begin, end, cluster_labels);

  // Update performance statistics
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time_);
  double total_time_ms = duration.count() / 1000.0;

  updateStatistics(num_hits, neutrons.size(), total_time_ms);

  return neutrons;
}

NeutronProcessingResults BasicNeutronProcessor::processHitsWithLabels(
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end) {
  start_time_ = std::chrono::high_resolution_clock::now();

  const size_t num_hits = std::distance(begin, end);

  if (num_hits == 0) {
    updateStatistics(0, 0, 0.0);
    return NeutronProcessingResults({});
  }

  // Create temporary copy for clustering
  std::vector<TDCHit> mutable_hits(begin, end);

  clusterer_->cluster(mutable_hits.begin(), mutable_hits.end());
  const std::vector<int>& cluster_labels = clusterer_->getClusterLabels();

  std::vector<TDCNeutron> neutrons =
      extractor_->extract(begin, end, cluster_labels);

  // Update performance statistics
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time_);
  double total_time_ms = duration.count() / 1000.0;

  updateStatistics(num_hits, neutrons.size(), total_time_ms);

  // Return both neutrons and cluster labels for diagnostics
  std::vector<int> labels_copy = cluster_labels;  // Copy the labels
  return NeutronProcessingResults(std::move(neutrons), std::move(labels_copy));
}

void BasicNeutronProcessor::configure(const NeutronProcessingConfig& config) {
  config.validate();
  config_ = config;

  // Reconfigure algorithms
  clusterer_->configure(config_.clustering);
  extractor_->configure(config_.extraction);

  reset();
}

std::string BasicNeutronProcessor::getHitClusteringAlgorithm() const {
  return clusterer_->getName();
}

std::string BasicNeutronProcessor::getNeutronExtractionAlgorithm() const {
  return extractor_->getName();
}

double BasicNeutronProcessor::getLastProcessingTimeMs() const {
  return last_stats_.total_processing_time_ms;
}

double BasicNeutronProcessor::getLastHitsPerSecond() const {
  return last_stats_.getHitsPerSecond();
}

double BasicNeutronProcessor::getLastNeutronEfficiency() const {
  return last_stats_.neutron_efficiency;
}

void BasicNeutronProcessor::reset() {
  clusterer_->reset();
  extractor_->reset();
  last_stats_ = ProcessingStatistics{};
}

void BasicNeutronProcessor::updateStatistics(size_t num_hits,
                                             size_t num_neutrons,
                                             double total_time_ms) {
  // Get clustering statistics
  ClusteringStatistics clustering_stats = clusterer_->getStatistics();

  // Get extraction statistics
  ExtractionStatistics extraction_stats = extractor_->getStatistics();

  // Combine into processing statistics
  last_stats_.total_hits_processed = num_hits;
  last_stats_.total_neutrons_produced = num_neutrons;
  last_stats_.total_clusters_found = clustering_stats.total_clusters;
  last_stats_.total_processing_time_ms = total_time_ms;

  // Phase timing (basic implementation doesn't separate phases precisely)
  last_stats_.analysis_time_ms =
      0.0;  // No analysis phase in basic implementation
  last_stats_.batching_time_ms = 0.0;  // No batching in basic implementation
  last_stats_.clustering_time_ms = clustering_stats.processing_time_ms;
  last_stats_.extraction_time_ms = extraction_stats.processing_time_ms;
  last_stats_.aggregation_time_ms =
      0.0;  // No aggregation in basic implementation

  // Parallel processing metrics (not applicable for basic implementation)
  last_stats_.num_workers_used = 1;       // Single-threaded
  last_stats_.num_batches_created = 1;    // Single batch
  last_stats_.parallel_efficiency = 1.0;  // Perfect for single-threaded
  last_stats_.load_balance_factor = 1.0;  // Perfect for single-threaded

  // Memory metrics (approximate)
  last_stats_.peak_memory_usage_mb =
      static_cast<double>(num_hits * sizeof(TDCHit)) / (1024.0 * 1024.0);
  last_stats_.memory_per_worker_mb =
      last_stats_.peak_memory_usage_mb;  // Single worker

  // Quality metrics
  if (num_hits > 0) {
    last_stats_.neutron_efficiency =
        static_cast<double>(num_neutrons) / num_hits;
  } else {
    last_stats_.neutron_efficiency = 0.0;
  }

  if (last_stats_.total_clusters_found > 0) {
    last_stats_.mean_cluster_size =
        static_cast<double>(num_hits - clustering_stats.unclustered_hits) /
        last_stats_.total_clusters_found;
  } else {
    last_stats_.mean_cluster_size = 0.0;
  }

  last_stats_.clusters_rejected = extraction_stats.rejected_clusters;
  last_stats_.duplicate_neutrons_removed =
      0;  // No deduplication in basic implementation
}

}  // namespace tdcsophiread