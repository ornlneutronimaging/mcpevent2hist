// TDCSophiread Cluster Processor Implementation
// Complete hits-to-neutrons processing pipeline

#include "tdc_cluster_processor.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>

#include "tdc_abs_clustering.h"
#include "tdc_centroid_fitting.h"

namespace tdcsophiread {

TDCClusterProcessor::TDCClusterProcessor()
    : TDCClusterProcessor(ClusteringConfig::venusDefaults()) {}

TDCClusterProcessor::TDCClusterProcessor(const ClusteringConfig& config)
    : config_(config),
      last_hit_count_(0),
      last_neutron_count_(0),
      last_processing_time_ms_(0.0) {
  initializeAlgorithms();
}

void TDCClusterProcessor::initializeAlgorithms() {
  // Initialize clustering algorithm based on configuration
  if (config_.clustering_algorithm == "abs") {
    clustering_algorithm_ = std::make_unique<ABSClustering>(config_.abs);
  } else {
    throw std::invalid_argument("Unsupported clustering algorithm: " +
                                config_.clustering_algorithm);
  }

  // Initialize peak fitting algorithm based on configuration
  if (config_.peak_fitting_algorithm == "centroid") {
    peak_fitting_algorithm_ =
        std::make_unique<CentroidPeakFitting>(config_.centroid);
  } else {
    throw std::invalid_argument("Unsupported peak fitting algorithm: " +
                                config_.peak_fitting_algorithm);
  }
}

void TDCClusterProcessor::configure(const ClusteringConfig& config) {
  config_ = config;
  initializeAlgorithms();
}

std::vector<TDCNeutron> TDCClusterProcessor::processHits(
    const std::vector<TDCHit>& hits) {
  start_time_ = std::chrono::high_resolution_clock::now();

  if (!config_.enable_clustering) {
    // If clustering is disabled, convert each hit directly to a neutron
    std::vector<TDCNeutron> neutrons;
    neutrons.reserve(hits.size());

    for (const auto& hit : hits) {
      TDCNeutron neutron;
      neutron.x = hit.x * config_.centroid.super_resolution_factor;
      neutron.y = hit.y * config_.centroid.super_resolution_factor;
      neutron.tof = hit.tof;
      neutron.tot = hit.tot;
      neutron.n_hits = 1;
      neutron.chip_id = hit.chip_id;
      neutrons.push_back(neutron);
    }

    updatePerformanceMetrics(hits.size(), neutrons.size());
    return neutrons;
  }

  if (hits.empty()) {
    updatePerformanceMetrics(0, 0);
    return {};
  }

  // Validate input hits
  if (!validateInputHits(hits)) {
    throw std::invalid_argument("Invalid input hits detected");
  }

  // Make a copy of hits for clustering (clustering modifies cluster_id)
  std::vector<TDCHit> clustered_hits = hits;

  // Phase 1: Clustering - assign cluster labels to hits
  clustering_algorithm_->fit(clustered_hits);

  // Phase 2: Peak fitting - extract neutrons from clusters
  std::vector<TDCNeutron> neutrons =
      peak_fitting_algorithm_->extractNeutrons(clustered_hits);

  updatePerformanceMetrics(hits.size(), neutrons.size());

  return neutrons;
}

std::vector<TDCNeutron> TDCClusterProcessor::processHitsWithProgress(
    const std::vector<TDCHit>& hits,
    const std::function<void(double)>& progress_callback) {
  if (progress_callback) {
    progress_callback(0.0);  // Start
  }

  if (!config_.enable_clustering) {
    // If clustering is disabled, convert each hit directly to a neutron
    std::vector<TDCNeutron> neutrons;
    neutrons.reserve(hits.size());

    for (const auto& hit : hits) {
      TDCNeutron neutron;
      neutron.x = hit.x * config_.centroid.super_resolution_factor;
      neutron.y = hit.y * config_.centroid.super_resolution_factor;
      neutron.tof = hit.tof;
      neutron.tot = hit.tot;
      neutron.n_hits = 1;
      neutron.chip_id = hit.chip_id;
      neutrons.push_back(neutron);
    }

    if (progress_callback) {
      progress_callback(1.0);  // Complete
    }
    updatePerformanceMetrics(hits.size(), neutrons.size());
    return neutrons;
  }

  if (hits.empty()) {
    if (progress_callback) {
      progress_callback(1.0);  // Complete
    }
    updatePerformanceMetrics(0, 0);
    return {};
  }

  start_time_ = std::chrono::high_resolution_clock::now();

  // Validate input
  if (!validateInputHits(hits)) {
    throw std::invalid_argument("Invalid input hits detected");
  }

  std::vector<TDCHit> clustered_hits = hits;

  if (progress_callback) {
    progress_callback(0.1);  // After validation
  }

  // Phase 1: Clustering
  clustering_algorithm_->fit(clustered_hits);

  if (progress_callback) {
    progress_callback(0.6);  // After clustering
  }

  // Phase 2: Peak fitting
  std::vector<TDCNeutron> neutrons =
      peak_fitting_algorithm_->extractNeutrons(clustered_hits);

  if (progress_callback) {
    progress_callback(1.0);  // Complete
  }

  updatePerformanceMetrics(hits.size(), neutrons.size());

  return neutrons;
}

std::vector<TDCNeutron> TDCClusterProcessor::processHitsInChunks(
    const std::vector<TDCHit>& hits, size_t chunk_size) {
  if (chunk_size == 0 || hits.size() <= chunk_size) {
    // Process all at once
    return processHits(hits);
  }

  std::vector<TDCNeutron> all_neutrons;
  size_t total_hits = 0;

  start_time_ = std::chrono::high_resolution_clock::now();

  // Process hits in chunks
  for (size_t start = 0; start < hits.size(); start += chunk_size) {
    size_t end = std::min(start + chunk_size, hits.size());
    std::vector<TDCHit> chunk(hits.begin() + start, hits.begin() + end);

    // Reset clustering algorithm for each chunk
    clustering_algorithm_->reset();
    // Note: IPeakFitting doesn't have reset(), but CentroidPeakFitting does
    if (auto* centroid_fitting =
            dynamic_cast<CentroidPeakFitting*>(peak_fitting_algorithm_.get())) {
      centroid_fitting->reset();
    }

    // Process chunk
    auto chunk_neutrons = processHits(chunk);

    // Accumulate results
    all_neutrons.insert(all_neutrons.end(), chunk_neutrons.begin(),
                        chunk_neutrons.end());
    total_hits += chunk.size();
  }

  // Update overall metrics
  updatePerformanceMetrics(total_hits, all_neutrons.size());

  return all_neutrons;
}

std::string TDCClusterProcessor::getClusteringAlgorithm() const {
  return config_.clustering_algorithm;
}

std::string TDCClusterProcessor::getPeakFittingAlgorithm() const {
  return config_.peak_fitting_algorithm;
}

double TDCClusterProcessor::getLastProcessingTimeMs() const {
  return last_processing_time_ms_;
}

double TDCClusterProcessor::getLastHitsPerSecond() const {
  if (last_processing_time_ms_ <= 0.0) {
    return 0.0;
  }
  return (last_hit_count_ * 1000.0) / last_processing_time_ms_;
}

double TDCClusterProcessor::getLastNeutronEfficiency() const {
  if (last_hit_count_ == 0) {
    return 0.0;
  }
  return static_cast<double>(last_neutron_count_) /
         static_cast<double>(last_hit_count_);
}

const ClusteringConfig& TDCClusterProcessor::getConfiguration() const {
  return config_;
}

IClusteringAlgorithm* TDCClusterProcessor::getClusteringAlgorithmPtr() const {
  return clustering_algorithm_.get();
}

IPeakFitting* TDCClusterProcessor::getPeakFittingAlgorithmPtr() const {
  return peak_fitting_algorithm_.get();
}

size_t TDCClusterProcessor::getLastNeutronCount() const {
  return last_neutron_count_;
}

NeutronStatistics TDCClusterProcessor::getLastStatistics() const {
  // This would require storing the last neutron results
  // For now, return empty statistics
  // TODO: Store last neutron results for statistics calculation
  return NeutronStatistics{};
}

bool TDCClusterProcessor::validateInputHits(
    const std::vector<TDCHit>& hits) const {
  for (const auto& hit : hits) {
    // Check for reasonable coordinate values
    if (hit.x > 4096 || hit.y > 4096) {
      return false;  // Coordinates too large
    }

    // Check for valid chip ID
    if (hit.chip_id > 3) {
      return false;  // Invalid chip ID for 2x2 layout
    }

    // Check for reasonable TOT values
    if (hit.tot > 65535) {
      return false;  // TOT overflow
    }
  }
  return true;
}

void TDCClusterProcessor::reset() {
  if (clustering_algorithm_) {
    clustering_algorithm_->reset();
  }
  if (peak_fitting_algorithm_) {
    // Note: IPeakFitting doesn't have reset(), but CentroidPeakFitting does
    if (auto* centroid_fitting =
            dynamic_cast<CentroidPeakFitting*>(peak_fitting_algorithm_.get())) {
      centroid_fitting->reset();
    }
  }
  last_hit_count_ = 0;
  last_neutron_count_ = 0;
  last_processing_time_ms_ = 0.0;
}

bool TDCClusterProcessor::isClusteringEnabled() const {
  return config_.enable_clustering;
}

std::map<uint8_t, std::vector<TDCNeutron>>
TDCClusterProcessor::processHitsByChip(const std::vector<TDCHit>& hits) {
  std::map<uint8_t, std::vector<TDCNeutron>> chip_neutrons;

  // Group hits by chip
  std::map<uint8_t, std::vector<TDCHit>> chip_hits;
  for (const auto& hit : hits) {
    chip_hits[hit.chip_id].push_back(hit);
  }

  // Process each chip separately
  for (auto& [chip_id, hits_for_chip] : chip_hits) {
    // Reset algorithms for each chip
    clustering_algorithm_->reset();
    // Note: IPeakFitting doesn't have reset(), but CentroidPeakFitting does
    if (auto* centroid_fitting =
            dynamic_cast<CentroidPeakFitting*>(peak_fitting_algorithm_.get())) {
      centroid_fitting->reset();
    }

    // Process hits for this chip
    auto neutrons = processHits(hits_for_chip);
    chip_neutrons[chip_id] = std::move(neutrons);
  }

  return chip_neutrons;
}

std::string TDCClusterProcessor::getProcessingSummary() const {
  std::ostringstream summary;
  summary << "TDC Cluster Processing Summary:\n";
  summary << "  Clustering Algorithm: " << config_.clustering_algorithm << "\n";
  summary << "  Peak Fitting Algorithm: " << config_.peak_fitting_algorithm
          << "\n";
  summary << "  Last Processing:\n";
  summary << "    Input Hits: " << last_hit_count_ << "\n";
  summary << "    Output Neutrons: " << last_neutron_count_ << "\n";
  summary << "    Processing Time: " << last_processing_time_ms_ << " ms\n";
  summary << "    Throughput: " << getLastHitsPerSecond() << " hits/sec\n";
  summary << "    Neutron Efficiency: " << (getLastNeutronEfficiency() * 100.0)
          << "%\n";
  summary << "  Configuration:\n";
  summary << "    Super-Resolution Factor: "
          << config_.centroid.super_resolution_factor << "x\n";
  summary << "    ABS Radius: " << config_.abs.radius << " pixels\n";
  summary << "    ABS Time Range: " << config_.abs.time_range_ns << " ns\n";
  return summary.str();
}

void TDCClusterProcessor::updatePerformanceMetrics(size_t hit_count,
                                                   size_t neutron_count) {
  end_time_ = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time_ - start_time_);

  last_hit_count_ = hit_count;
  last_neutron_count_ = neutron_count;
  last_processing_time_ms_ = duration.count() / 1000.0;
}

// Utility functions implementation

std::vector<TDCHit> ClusterProcessingUtils::filterHitsByChip(
    const std::vector<TDCHit>& hits, uint8_t chip_id) {
  std::vector<TDCHit> filtered_hits;
  filtered_hits.reserve(hits.size() / 4);  // Rough estimate

  for (const auto& hit : hits) {
    if (hit.chip_id == chip_id) {
      filtered_hits.push_back(hit);
    }
  }

  return filtered_hits;
}

void ClusterProcessingUtils::sortHitsByTimestamp(std::vector<TDCHit>& hits) {
  std::sort(hits.begin(), hits.end(), [](const TDCHit& a, const TDCHit& b) {
    return a.tof < b.tof;  // Sort by time-of-flight
  });
}

std::vector<TDCHit> ClusterProcessingUtils::filterValidClusteredHits(
    const std::vector<TDCHit>& hits) {
  std::vector<TDCHit> filtered_hits;
  filtered_hits.reserve(hits.size());

  for (const auto& hit : hits) {
    if (hit.cluster_id >= 0) {
      filtered_hits.push_back(hit);
    }
  }

  return filtered_hits;
}

std::map<int, size_t> ClusterProcessingUtils::calculateClusterSizes(
    const std::vector<TDCHit>& hits) {
  std::map<int, size_t> cluster_sizes;

  for (const auto& hit : hits) {
    if (hit.cluster_id >= 0) {
      cluster_sizes[static_cast<int>(hit.cluster_id)]++;
    }
  }

  return cluster_sizes;
}

bool ClusterProcessingUtils::validateClusterLabels(
    const std::vector<TDCHit>& hits) {
  // Check that cluster IDs are reasonable
  int8_t max_cluster_id = -1;
  int8_t min_cluster_id = INT8_MAX;

  for (const auto& hit : hits) {
    if (hit.cluster_id >= 0) {
      max_cluster_id = std::max(max_cluster_id, hit.cluster_id);
      min_cluster_id = std::min(min_cluster_id, hit.cluster_id);
    }
  }

  // If no valid clusters found
  if (max_cluster_id < 0) {
    return true;  // No clusters is valid
  }

  // Check that cluster IDs are reasonably dense (no huge gaps)
  int cluster_range = max_cluster_id - min_cluster_id + 1;
  size_t valid_hits = filterValidClusteredHits(hits).size();

  // If there are more cluster IDs than hits, something is wrong
  return cluster_range <= static_cast<int>(valid_hits);
}

std::vector<TDCHit> ClusterProcessingUtils::createHitSubset(
    const std::vector<TDCHit>& hits, size_t max_hits, bool random_sample) {
  if (hits.size() <= max_hits) {
    return hits;  // Return all hits if within limit
  }

  std::vector<TDCHit> subset;
  subset.reserve(max_hits);

  if (random_sample) {
    // Random sampling
    std::vector<size_t> indices(hits.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);

    for (size_t i = 0; i < max_hits; ++i) {
      subset.push_back(hits[indices[i]]);
    }
  } else {
    // Take first N hits
    subset.assign(hits.begin(), hits.begin() + max_hits);
  }

  return subset;
}

size_t ClusterProcessingUtils::estimateMemoryUsage(
    size_t hit_count, const ClusteringConfig& config) {
  // Rough estimation based on data structures
  size_t hit_size = sizeof(TDCHit);
  size_t neutron_size = sizeof(TDCNeutron);

  // Input hits + copy for clustering + neutron output (worst case: 1 neutron
  // per hit)
  size_t base_memory = hit_count * (2 * hit_size + neutron_size);

  // Algorithm-specific overhead
  size_t algorithm_overhead = 0;
  if (config.clustering_algorithm == "abs") {
    // ABS uses fixed-size cluster pool
    algorithm_overhead = config.abs.max_clusters * 1024;  // Rough estimate
  }

  return base_memory + algorithm_overhead;
}

size_t ClusterProcessingUtils::getRecommendedChunkSize(
    size_t total_hits, size_t available_memory_mb,
    const ClusteringConfig& config) {
  size_t available_bytes = available_memory_mb * 1024 * 1024;

  // Use 80% of available memory for safety
  size_t target_memory = static_cast<size_t>(available_bytes * 0.8);

  // Estimate hits per chunk based on memory usage
  size_t memory_per_hit = estimateMemoryUsage(1, config);
  size_t max_hits_for_memory = target_memory / memory_per_hit;

  // Don't exceed total hits
  size_t chunk_size = std::min(max_hits_for_memory, total_hits);

  // Ensure minimum chunk size for efficiency
  const size_t min_chunk_size = 1000;
  chunk_size = std::max(chunk_size, min_chunk_size);

  return chunk_size;
}

}  // namespace tdcsophiread