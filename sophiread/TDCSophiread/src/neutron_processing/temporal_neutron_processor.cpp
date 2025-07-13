// TDCSophiread Temporal Neutron Processor Implementation
// Parallel neutron processing with worker pool architecture

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <chrono>
#include <execution>
#include <stdexcept>
#include <thread>

#include "neutron_processing/neutron_factories.h"
#include "neutron_processing/neutron_processing.h"

namespace tdcsophiread {

TemporalNeutronProcessor::TemporalNeutronProcessor()
    : config_(NeutronProcessingConfig::venusDefaults()), last_stats_() {
  initializeWorkers();
}

TemporalNeutronProcessor::TemporalNeutronProcessor(
    const NeutronProcessingConfig& config)
    : config_(config), last_stats_() {
  config_.validate();
  initializeWorkers();
}

void TemporalNeutronProcessor::initializeWorkers() {
  // Determine number of workers
  size_t num_workers = config_.temporal.num_workers;
  if (num_workers == 0) {
    num_workers = std::thread::hardware_concurrency();
    if (num_workers == 0) num_workers = 4;  // Fallback
  }

  workers_.clear();
  workers_.resize(num_workers);

  // Initialize each worker with its own algorithm instances
  for (size_t i = 0; i < num_workers; ++i) {
    workers_[i].clusterer = HitClusteringFactory::create(
        config_.clustering.algorithm, config_.clustering);
    workers_[i].extractor = NeutronExtractionFactory::create(
        config_.extraction.algorithm, config_.extraction);
    workers_[i].cluster_id_offset = 0;
  }
}

std::vector<TDCNeutron> TemporalNeutronProcessor::processHits(
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end) {
  auto start_time = std::chrono::high_resolution_clock::now();

  const size_t num_hits = std::distance(begin, end);

  if (num_hits == 0) {
    updateStatistics(0, 0, 0.0, 1, false);
    return {};
  }

  // Phase 1: Statistical analysis to determine optimal batch boundaries
  auto stats = TemporalBatching::analyzeHitDistribution(begin, end);

  // Phase 2: Create local vector for temporal batching (necessary for zero-copy
  // batches)
  std::vector<TDCHit> local_hits(begin, end);
  auto batches = TemporalBatching::createStatisticalBatches(
      &local_hits, local_hits.begin(), local_hits.end(), stats);

  if (batches.empty()) {
    updateStatistics(num_hits, 0, 0.0, 1, false);
    return {};
  }

  // Phase 3: Calculate cluster ID offsets for proper parallel processing
  calculateClusterIdOffsets(batches);

  // Phase 4: Process batches in parallel using TBB
  tbb::parallel_for(tbb::blocked_range<size_t>(0, batches.size()),
                    [&](const tbb::blocked_range<size_t>& range) {
                      for (size_t i = range.begin(); i != range.end(); ++i) {
                        size_t worker_id = i % workers_.size();
                        processBatch(batches[i], worker_id, false);
                      }
                    });

  // Phase 5: Combine results from all workers
  auto neutrons = combineNeutronResults();

  // Phase 6: Remove duplicates in overlap regions if enabled
  if (config_.temporal.enable_deduplication) {
    deduplicateNeutrons(neutrons);
  }

  // Update performance statistics
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);
  double total_time_ms = duration.count() / 1000.0;

  updateStatistics(num_hits, neutrons.size(), total_time_ms, batches.size(),
                   false);

  return neutrons;
}

NeutronProcessingResults TemporalNeutronProcessor::processHitsWithLabels(
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end) {
  auto start_time = std::chrono::high_resolution_clock::now();

  const size_t num_hits = std::distance(begin, end);

  if (num_hits == 0) {
    updateStatistics(0, 0, 0.0, 1, true);
    return NeutronProcessingResults({});
  }

  // Phase 1: Statistical analysis
  auto stats = TemporalBatching::analyzeHitDistribution(begin, end);

  // Phase 2: Create local vector for temporal batching
  std::vector<TDCHit> local_hits(begin, end);
  auto batches = TemporalBatching::createStatisticalBatches(
      &local_hits, local_hits.begin(), local_hits.end(), stats);

  if (batches.empty()) {
    updateStatistics(num_hits, 0, 0.0, 1, true);
    return NeutronProcessingResults({});
  }

  // Phase 3: Calculate cluster ID offsets
  calculateClusterIdOffsets(batches);

  // Phase 4: Process batches in parallel (with label tracking)
  tbb::parallel_for(tbb::blocked_range<size_t>(0, batches.size()),
                    [&](const tbb::blocked_range<size_t>& range) {
                      for (size_t i = range.begin(); i != range.end(); ++i) {
                        size_t worker_id = i % workers_.size();
                        processBatch(batches[i], worker_id, true);
                      }
                    });

  // Phase 5: Combine results
  auto neutrons = combineNeutronResults();
  auto cluster_labels = combineClusterLabels(begin, end, batches);

  // Phase 6: Deduplication if enabled
  if (config_.temporal.enable_deduplication) {
    deduplicateNeutrons(neutrons);
  }

  // Update statistics
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);
  double total_time_ms = duration.count() / 1000.0;

  updateStatistics(num_hits, neutrons.size(), total_time_ms, batches.size(),
                   true);

  return NeutronProcessingResults(std::move(neutrons),
                                  std::move(cluster_labels));
}

void TemporalNeutronProcessor::processBatch(const HitBatch& batch,
                                            size_t worker_id,
                                            bool track_labels) {
  if (worker_id >= workers_.size() || !batch.isValid()) {
    return;
  }

  Worker& worker = workers_[worker_id];

  // Convert hit range to mutable for clustering
  auto batch_begin = batch.begin();
  auto batch_end = batch.end();
  std::vector<TDCHit> batch_hits(batch_begin, batch_end);

  // Apply cluster ID offset before clustering
  worker.clusterer->reset();
  worker.clusterer->cluster(batch_hits.begin(), batch_hits.end());

  // Get cluster labels and apply offset
  const auto& raw_labels = worker.clusterer->getClusterLabels();

  if (track_labels) {
    worker.cluster_label_results.assign(raw_labels.begin(), raw_labels.end());
    // Apply cluster ID offset to labels
    for (int& label : worker.cluster_label_results) {
      if (label >= 0) {
        label += worker.cluster_id_offset;
      }
    }
  }

  // Extract neutrons using const iterators on original data
  worker.extractor->reset();

  // Apply offset to cluster labels for neutron extraction
  std::vector<int> offset_labels = raw_labels;
  for (int& label : offset_labels) {
    if (label >= 0) {
      label += worker.cluster_id_offset;
    }
  }

  worker.neutron_results = worker.extractor->extract(
      batch_hits.begin(), batch_hits.end(), offset_labels);
}

std::vector<TDCNeutron> TemporalNeutronProcessor::combineNeutronResults() {
  std::vector<TDCNeutron> combined_neutrons;

  // Estimate total size to avoid reallocations
  size_t total_neutrons = 0;
  for (const auto& worker : workers_) {
    total_neutrons += worker.neutron_results.size();
  }
  combined_neutrons.reserve(total_neutrons);

  // Combine results from all workers
  for (const auto& worker : workers_) {
    combined_neutrons.insert(combined_neutrons.end(),
                             worker.neutron_results.begin(),
                             worker.neutron_results.end());
  }

  return combined_neutrons;
}

std::vector<int> TemporalNeutronProcessor::combineClusterLabels(
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end,
    const std::vector<HitBatch>& batches) {
  const size_t num_hits = std::distance(begin, end);
  std::vector<int> combined_labels(num_hits, -1);

  // Map batch results back to original hit indices
  for (size_t batch_idx = 0; batch_idx < batches.size(); ++batch_idx) {
    const auto& batch = batches[batch_idx];
    size_t worker_id = batch_idx % workers_.size();
    const auto& worker_labels = workers_[worker_id].cluster_label_results;

    if (worker_labels.size() != batch.size()) {
      continue;  // Skip if size mismatch
    }

    // Copy labels for non-overlap regions (later batches overwrite overlaps)
    size_t batch_hit_idx = 0;
    for (size_t global_idx = batch.start_index;
         global_idx < batch.end_index && batch_hit_idx < worker_labels.size();
         ++global_idx, ++batch_hit_idx) {
      size_t local_idx = global_idx;
      if (local_idx < num_hits) {
        combined_labels[local_idx] = worker_labels[batch_hit_idx];
      }
    }
  }

  return combined_labels;
}

void TemporalNeutronProcessor::deduplicateNeutrons(
    std::vector<TDCNeutron>& neutrons) {
  if (neutrons.size() <= 1) return;

  const double tolerance = config_.temporal.deduplication_tolerance;
  const double tolerance_sq = tolerance * tolerance;

  std::vector<bool> to_remove(neutrons.size(), false);

  // Mark duplicates (simple O(n²) approach for now)
  for (size_t i = 0; i < neutrons.size(); ++i) {
    if (to_remove[i]) continue;

    for (size_t j = i + 1; j < neutrons.size(); ++j) {
      if (to_remove[j]) continue;

      // Check spatial distance
      double dx = neutrons[i].x - neutrons[j].x;
      double dy = neutrons[i].y - neutrons[j].y;
      double dist_sq = dx * dx + dy * dy;

      if (dist_sq <= tolerance_sq) {
        // Check temporal distance (within same pulse)
        double dt = std::abs(static_cast<double>(neutrons[i].tof) -
                             static_cast<double>(neutrons[j].tof));
        if (dt < 16670000.0) {  // Within same 16.67ms pulse
          to_remove[j] = true;  // Remove later neutron
        }
      }
    }
  }

  // Remove marked neutrons
  auto new_end = std::remove_if(
      neutrons.begin(), neutrons.end(), [&](const TDCNeutron& n) {
        size_t idx = &n - &neutrons[0];
        return idx < to_remove.size() && to_remove[idx];
      });
  neutrons.erase(new_end, neutrons.end());
}

void TemporalNeutronProcessor::calculateClusterIdOffsets(
    const std::vector<HitBatch>& batches) {
  int next_offset = 0;

  for (size_t i = 0; i < batches.size(); ++i) {
    size_t worker_id = i % workers_.size();
    workers_[worker_id].cluster_id_offset = next_offset;

    // Estimate max clusters for this batch (rough upper bound)
    size_t batch_hits = batches[i].size();
    int estimated_clusters =
        static_cast<int>(batch_hits / 2) + 1;  // Conservative estimate
    next_offset += estimated_clusters;
  }
}

void TemporalNeutronProcessor::configure(
    const NeutronProcessingConfig& config) {
  config.validate();
  config_ = config;
  initializeWorkers();
  reset();
}

std::string TemporalNeutronProcessor::getHitClusteringAlgorithm() const {
  if (!workers_.empty() && workers_[0].clusterer) {
    return workers_[0].clusterer->getName();
  }
  return config_.clustering.algorithm;
}

std::string TemporalNeutronProcessor::getNeutronExtractionAlgorithm() const {
  if (!workers_.empty() && workers_[0].extractor) {
    return workers_[0].extractor->getName();
  }
  return config_.extraction.algorithm;
}

double TemporalNeutronProcessor::getLastProcessingTimeMs() const {
  return last_stats_.total_processing_time_ms;
}

double TemporalNeutronProcessor::getLastHitsPerSecond() const {
  return last_stats_.getHitsPerSecond();
}

double TemporalNeutronProcessor::getLastNeutronEfficiency() const {
  return last_stats_.neutron_efficiency;
}

void TemporalNeutronProcessor::reset() {
  for (auto& worker : workers_) {
    worker.reset();
  }
  last_stats_ = ProcessingStatistics{};
}

void TemporalNeutronProcessor::updateStatistics(size_t num_hits,
                                                size_t num_neutrons,
                                                double total_time_ms,
                                                size_t num_batches,
                                                bool /* with_labels */) {
  last_stats_.total_hits_processed = num_hits;
  last_stats_.total_neutrons_produced = num_neutrons;
  last_stats_.total_processing_time_ms = total_time_ms;
  last_stats_.num_batches_created = num_batches;
  last_stats_.num_workers_used = workers_.size();

  // Calculate timing breakdown (approximate)
  last_stats_.analysis_time_ms = total_time_ms * 0.1;    // ~10% for analysis
  last_stats_.batching_time_ms = total_time_ms * 0.05;   // ~5% for batching
  last_stats_.clustering_time_ms = total_time_ms * 0.4;  // ~40% for clustering
  last_stats_.extraction_time_ms = total_time_ms * 0.3;  // ~30% for extraction
  last_stats_.aggregation_time_ms =
      total_time_ms * 0.15;  // ~15% for aggregation

  // Parallel efficiency metrics
  if (workers_.size() > 1) {
    last_stats_.parallel_efficiency =
        std::min(1.0, static_cast<double>(num_batches) / workers_.size());
    last_stats_.load_balance_factor =
        1.0;  // TODO: Implement proper load balance calculation
  } else {
    last_stats_.parallel_efficiency = 1.0;
    last_stats_.load_balance_factor = 1.0;
  }

  // Quality metrics
  if (num_hits > 0) {
    last_stats_.neutron_efficiency =
        static_cast<double>(num_neutrons) / num_hits;
  } else {
    last_stats_.neutron_efficiency = 0.0;
  }

  // Memory metrics (approximate)
  last_stats_.peak_memory_usage_mb =
      static_cast<double>(num_hits * sizeof(TDCHit) * workers_.size()) /
      (1024.0 * 1024.0);
  last_stats_.memory_per_worker_mb =
      last_stats_.peak_memory_usage_mb / workers_.size();

  // Aggregate clustering statistics from workers
  size_t total_clusters = 0;
  for (const auto& worker : workers_) {
    if (worker.clusterer) {
      auto worker_stats = worker.clusterer->getStatistics();
      total_clusters += worker_stats.total_clusters;
    }
  }
  last_stats_.total_clusters_found = total_clusters;

  if (total_clusters > 0) {
    last_stats_.mean_cluster_size =
        static_cast<double>(num_hits) / total_clusters;
  } else {
    last_stats_.mean_cluster_size = 0.0;
  }
}

}  // namespace tdcsophiread