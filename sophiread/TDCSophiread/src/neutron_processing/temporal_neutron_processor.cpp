// TDCSophiread Temporal Neutron Processor Implementation
// Stateless parallel neutron processing with pre-allocated algorithm pool

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/task_arena.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <stdexcept>

#include "neutron_processing/hit_clustering.h"  // For TemporalBatching namespace
#include "neutron_processing/neutron_factories.h"
#include "neutron_processing/neutron_processing.h"

namespace tdcsophiread {

TemporalNeutronProcessor::TemporalNeutronProcessor()
    : config_(NeutronProcessingConfig::venusDefaults()), last_stats_() {
  initializeAlgorithmPool();
}

TemporalNeutronProcessor::TemporalNeutronProcessor(
    const NeutronProcessingConfig& config)
    : config_(config), last_stats_() {
  config_.validate();
  initializeAlgorithmPool();
}

TemporalNeutronProcessor::~TemporalNeutronProcessor() = default;

void TemporalNeutronProcessor::configure(
    const NeutronProcessingConfig& config) {
  config.validate();
  config_ = config;
  initializeAlgorithmPool();
}

void TemporalNeutronProcessor::initializeAlgorithmPool() {
  // Determine number of algorithm sets needed
  size_t num_threads = config_.temporal.num_workers;
  if (num_threads == 0) {
    // Auto-detect based on hardware
    num_threads = tbb::this_task_arena::max_concurrency();
    if (num_threads == 0) {
      num_threads = 4;  // Fallback
    }
  }

  // Clear existing pool
  algorithm_pool_.clear();
  algorithm_pool_.reserve(num_threads);

  // Create algorithm instances for each potential thread
  for (size_t i = 0; i < num_threads; ++i) {
    AlgorithmSet set;
    set.clusterer = HitClusteringFactory::create(config_.clustering.algorithm,
                                                 config_.clustering);
    set.extractor = NeutronExtractionFactory::create(
        config_.extraction.algorithm, config_.extraction);
    algorithm_pool_.push_back(std::move(set));
  }
}

std::vector<TDCNeutron> TemporalNeutronProcessor::processHits(
    const std::vector<TDCHit>& hits, size_t start_offset, size_t end_offset) {
  auto start_time = std::chrono::high_resolution_clock::now();

  // Validate and adjust offsets
  if (start_offset >= hits.size()) {
    updateStatistics(0, 0, 0.0, 0);
    return {};
  }

  end_offset = std::min(end_offset, hits.size());
  const size_t num_hits = end_offset - start_offset;

  if (num_hits == 0) {
    updateStatistics(0, 0, 0.0, 0);
    return {};
  }

  // Create fixed-size batches with simple overlap
  auto begin_iter = hits.begin() + start_offset;
  auto end_iter = hits.begin() + end_offset;

  // Use larger batch size for better performance
  // For large datasets (>10M hits), use larger batches to reduce overhead
  size_t default_batch_size = num_hits > 10000000 ? 500000 : 50000;
  size_t batch_size =
      std::min(default_batch_size, config_.temporal.max_batch_size);
  size_t overlap_size =
      0;  // Disabled - overlap causes index bugs with current implementation

  auto batches = TemporalBatching::createFixedSizeBatches(
      &hits, begin_iter, end_iter, batch_size, overlap_size);

  if (batches.empty()) {
    updateStatistics(num_hits, 0, 0.0, 0);
    return {};
  }

  // Calculate cluster ID offsets
  calculateClusterIdOffsets(batches);

  // Process batches in parallel
  processBatchesParallel(batches);

  // Collect results
  auto neutrons = collectNeutronResults(batches);

  // Remove duplicates if enabled (disabled by default)
  if (config_.temporal.enable_deduplication) {
    neutrons = deduplicateNeutrons(std::move(neutrons));
  }

  // Update statistics
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);
  double total_time_ms = duration.count() / 1000.0;

  updateStatistics(num_hits, neutrons.size(), total_time_ms, batches.size());

  return neutrons;
}

void TemporalNeutronProcessor::processBatchesParallel(
    std::vector<HitBatch>& batches) {
  if (algorithm_pool_.empty()) {
    throw std::runtime_error("Algorithm pool not initialized");
  }

  // Use blocked_range for better load balancing
  tbb::parallel_for(
      tbb::blocked_range<size_t>(0, batches.size()),
      [this, &batches](const tbb::blocked_range<size_t>& range) {
        // Get thread-specific algorithm set
        size_t thread_idx = tbb::this_task_arena::current_thread_index();
        if (thread_idx >= algorithm_pool_.size()) {
          thread_idx = thread_idx % algorithm_pool_.size();
        }

        const auto& algorithms = algorithm_pool_[thread_idx];

        // Process all batches in this thread's range
        for (size_t batch_idx = range.begin(); batch_idx != range.end();
             ++batch_idx) {
          auto& batch = batches[batch_idx];

          // FIXED - Create state inside TBB loop (per-thread, released
          // immediately)
          batch.initializeResults();
          batch.clustering_state = algorithms.clusterer->createState();

          try {
            processSingleBatch(batch, algorithms);
          } catch (const std::exception& e) {
            // Silently continue processing other batches
            // Error handling should be done at a higher level if needed
          }

          // Release state immediately after processing (saves memory)
          batch.clustering_state.reset();
        }
      });
}

void TemporalNeutronProcessor::processSingleBatch(
    HitBatch& batch, const AlgorithmSet& algorithms) {
  if (!batch.isValid()) {
    return;
  }

  // Perform clustering on the main batch range only
  // (overlap regions cause index mismatches with current ABS implementation)
  algorithms.clusterer->cluster(batch.begin(), batch.end(),
                                *batch.clustering_state, batch.cluster_labels);

  // Apply cluster ID offset for global uniqueness
  if (batch.cluster_id_offset > 0) {
    for (int& label : batch.cluster_labels) {
      if (label >= 0) {
        label += batch.cluster_id_offset;
      }
    }
  }

  // Extract neutrons from the main batch range
  batch.neutron_results = algorithms.extractor->extract(
      batch.begin(), batch.end(), batch.cluster_labels);
}

void TemporalNeutronProcessor::calculateClusterIdOffsets(
    std::vector<HitBatch>& batches) {
  int current_offset = 0;

  for (auto& batch : batches) {
    batch.cluster_id_offset = current_offset;

    // Estimate clusters based on hit density
    // Conservative estimate: 1 cluster per 10 hits
    size_t estimated_clusters = (batch.size() / 10) + 1;
    current_offset += static_cast<int>(estimated_clusters);
  }
}

std::vector<TDCNeutron> TemporalNeutronProcessor::collectNeutronResults(
    const std::vector<HitBatch>& batches) {
  // Calculate total neutrons for pre-allocation
  size_t total_neutrons = 0;
  for (const auto& batch : batches) {
    total_neutrons += batch.neutron_results.size();
  }

  // Collect all neutrons
  std::vector<TDCNeutron> neutrons;
  neutrons.reserve(total_neutrons);

  for (const auto& batch : batches) {
    neutrons.insert(neutrons.end(), batch.neutron_results.begin(),
                    batch.neutron_results.end());
  }

  return neutrons;
}

std::vector<TDCNeutron> TemporalNeutronProcessor::deduplicateNeutrons(
    std::vector<TDCNeutron> neutrons) {
  if (neutrons.size() <= 1) {
    return neutrons;
  }

  // Sort by position for spatial deduplication
  std::sort(neutrons.begin(), neutrons.end(),
            [](const TDCNeutron& a, const TDCNeutron& b) {
              if (a.x != b.x) return a.x < b.x;
              if (a.y != b.y) return a.y < b.y;
              return a.tof < b.tof;
            });

  // Remove duplicates within tolerance
  std::vector<TDCNeutron> unique_neutrons;
  unique_neutrons.reserve(neutrons.size());

  const double spatial_tol = config_.temporal.deduplication_tolerance;
  const uint32_t temporal_tol = static_cast<uint32_t>(
      config_.clustering.abs.neutron_correlation_window / 25.0);

  for (const auto& neutron : neutrons) {
    bool is_duplicate = false;

    // Check against recently added neutrons (only need to check nearby ones)
    for (auto it = unique_neutrons.rbegin(); it != unique_neutrons.rend();
         ++it) {
      // If we've moved too far in X, can stop checking
      if (std::abs(neutron.x - it->x) > spatial_tol) {
        break;
      }

      // Check full spatial and temporal proximity
      if (std::abs(neutron.x - it->x) < spatial_tol &&
          std::abs(neutron.y - it->y) < spatial_tol &&
          std::abs(static_cast<int32_t>(neutron.tof - it->tof)) <
              static_cast<int32_t>(temporal_tol)) {
        is_duplicate = true;
        break;
      }
    }

    if (!is_duplicate) {
      unique_neutrons.push_back(neutron);
    }
  }

  return unique_neutrons;
}

void TemporalNeutronProcessor::updateStatistics(size_t hits_processed,
                                                size_t neutrons_found,
                                                double processing_time_ms,
                                                size_t num_batches) {
  last_stats_.total_hits_processed = hits_processed;
  last_stats_.total_neutrons_produced = neutrons_found;
  last_stats_.total_processing_time_ms = processing_time_ms;
  last_stats_.processing_time_ms = processing_time_ms;

  if (processing_time_ms > 0 && hits_processed > 0) {
    last_stats_.hits_per_second =
        (hits_processed / processing_time_ms) * 1000.0;
    last_stats_.neutrons_per_second =
        (neutrons_found / processing_time_ms) * 1000.0;
  } else {
    last_stats_.hits_per_second = 0.0;
    last_stats_.neutrons_per_second = 0.0;
  }

  last_stats_.neutron_efficiency =
      (hits_processed > 0)
          ? static_cast<double>(neutrons_found) / hits_processed
          : 0.0;

  // Additional temporal processing stats
  last_stats_.num_batches = num_batches;
  last_stats_.avg_batch_size =
      (num_batches > 0) ? static_cast<double>(hits_processed) / num_batches
                        : 0.0;
}

// Interface method implementations

NeutronProcessingResults TemporalNeutronProcessor::processHitsWithLabels(
    const std::vector<TDCHit>& hits, size_t start_offset, size_t end_offset) {
  // For label tracking, we need to implement a more complex result aggregation
  // For now, just process without labels using the new zero-copy interface
  auto neutrons = processHits(hits, start_offset, end_offset);
  return NeutronProcessingResults(std::move(neutrons));
}

std::string TemporalNeutronProcessor::getHitClusteringAlgorithm() const {
  return config_.clustering.algorithm;
}

std::string TemporalNeutronProcessor::getNeutronExtractionAlgorithm() const {
  return config_.extraction.algorithm;
}

void TemporalNeutronProcessor::reset() {
  // Nothing to reset in stateless design
  // Algorithm instances maintain no state between calls
}

double TemporalNeutronProcessor::getLastProcessingTimeMs() const {
  return last_stats_.processing_time_ms;
}

double TemporalNeutronProcessor::getLastHitsPerSecond() const {
  return last_stats_.hits_per_second;
}

double TemporalNeutronProcessor::getLastNeutronEfficiency() const {
  return last_stats_.neutron_efficiency;
}

}  // namespace tdcsophiread