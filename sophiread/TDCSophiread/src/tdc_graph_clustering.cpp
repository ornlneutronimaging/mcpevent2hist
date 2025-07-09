// TDCSophiread Graph-Based Clustering Implementation
// High-performance parallel spatial-temporal clustering for neutron detection

#include "tdc_graph_clustering.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <unordered_set>

#include "tdc_centroid_fitting.h"
#include "tdc_clustering_config.h"

// TBB headers for parallelization
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <atomic>

namespace tdcsophiread {

void GraphConfig::validate() const {
  if (radius <= 0.0) {
    throw std::invalid_argument("Graph clustering radius must be positive");
  }
  if (neutron_correlation_window <= 0.0) {
    throw std::invalid_argument(
        "Graph neutron correlation window must be positive");
  }
  if (grid_size <= 0.0) {
    throw std::invalid_argument("Graph grid size must be positive");
  }
  if (min_cluster_size == 0) {
    throw std::invalid_argument(
        "Graph minimum cluster size must be at least 1");
  }
}

GraphClustering::GraphClustering(const GraphConfig& config)
    : config_(config),
      hits_ptr_(nullptr),
      union_find_size_(0),
      edge_count_(0),
      next_cluster_label_(0) {
  config_.validate();
  reset();
}

void GraphClustering::configure(const ClusteringConfig& config) {
  // Use properly configured GraphConfig
  config_ = config.graph;
}

size_t GraphClustering::fit(std::vector<TDCHit>& hits) {
  start_time_ = std::chrono::high_resolution_clock::now();

  // Reset state for new clustering operation
  reset();
  hits_ptr_ = &hits;

  if (hits.empty()) {
    updateStatistics();
    return 0;
  }

  // Phase 1: Build spatial hash table for efficient neighbor finding
  auto hash_start = std::chrono::high_resolution_clock::now();
  buildSpatialHash();
  auto hash_end = std::chrono::high_resolution_clock::now();
  stats_.graph_construction_ms =
      std::chrono::duration_cast<std::chrono::microseconds>(hash_end -
                                                            hash_start)
          .count() /
      1000.0;

  // Phase 2: Construct graph edges based on spatial-temporal criteria
  auto graph_start = std::chrono::high_resolution_clock::now();
  if (hits.size() >= config_.parallel_threshold) {
    constructGraph();  // Use parallel implementation
  } else {
    constructGraphSequential();  // Use sequential implementation
  }
  auto graph_end = std::chrono::high_resolution_clock::now();
  stats_.graph_construction_ms +=
      std::chrono::duration_cast<std::chrono::microseconds>(graph_end -
                                                            graph_start)
          .count() /
      1000.0;

  // Phase 3: Find connected components (each becomes a cluster)
  auto components_start = std::chrono::high_resolution_clock::now();
  size_t num_clusters = findConnectedComponents();
  auto components_end = std::chrono::high_resolution_clock::now();
  stats_.connected_components_ms =
      std::chrono::duration_cast<std::chrono::microseconds>(components_end -
                                                            components_start)
          .count() /
      1000.0;

  // Phase 4: Write cluster labels back to hits
  for (size_t i = 0; i < hits.size(); ++i) {
    hits[i].cluster_id = cluster_labels_[i];
  }

  // Update statistics and return
  updateStatistics();
  return num_clusters;
}

const std::vector<int>& GraphClustering::getClusterLabels() const {
  return cluster_labels_;
}

std::string GraphClustering::getName() const { return "graph"; }

void GraphClustering::reset() {
  clearInternalState();
  next_cluster_label_ = 0;
  edge_count_.store(0);
  stats_ = ClusteringStats{};
}

size_t GraphClustering::getLastHitCount() const { return stats_.total_hits; }

GraphClustering::ClusteringStats GraphClustering::getStatistics() const {
  return stats_;
}

void GraphClustering::updateConfig(const GraphConfig& config) {
  config_ = config;
  config_.validate();
}

GraphClustering::SpatialKey GraphClustering::encodeSpatialKey(
    uint16_t x, uint16_t y) const {
  // Encode (grid_x, grid_y) into single 32-bit key
  uint32_t grid_x = static_cast<uint32_t>(x / config_.grid_size);
  uint32_t grid_y = static_cast<uint32_t>(y / config_.grid_size);

  // Pack into 32-bit key: 16 bits each for x and y grid coordinates
  return (grid_x << 16) | grid_y;
}

std::vector<GraphClustering::SpatialKey> GraphClustering::getNeighborCells(
    uint16_t x, uint16_t y) const {
  std::vector<SpatialKey> neighbors;
  neighbors.reserve(9);  // Maximum 9 cells in 3x3 neighborhood

  int32_t grid_x = static_cast<int32_t>(x / config_.grid_size);
  int32_t grid_y = static_cast<int32_t>(y / config_.grid_size);

  // Check 3x3 neighborhood around current cell
  for (int32_t dx = -1; dx <= 1; ++dx) {
    for (int32_t dy = -1; dy <= 1; ++dy) {
      int32_t neighbor_x = grid_x + dx;
      int32_t neighbor_y = grid_y + dy;

      // Skip negative coordinates
      if (neighbor_x >= 0 && neighbor_y >= 0) {
        SpatialKey key = (static_cast<uint32_t>(neighbor_x) << 16) |
                         static_cast<uint32_t>(neighbor_y);
        neighbors.push_back(key);
      }
    }
  }

  return neighbors;
}

void GraphClustering::buildSpatialHash() {
  if (!config_.enable_spatial_hash || !hits_ptr_) {
    return;
  }

  spatial_hash_.clear();
  spatial_hash_.reserve(hits_ptr_->size() /
                        4);  // Rough estimate of unique grid cells

  // Sequential approach for correctness
  for (size_t hit_index = 0; hit_index < hits_ptr_->size(); ++hit_index) {
    const TDCHit& hit = (*hits_ptr_)[hit_index];
    SpatialKey key = encodeSpatialKey(hit.x, hit.y);
    spatial_hash_[key].push_back(hit_index);
  }

  // Update statistics
  stats_.spatial_hash_buckets = spatial_hash_.size();
  stats_.spatial_hash_load_factor =
      static_cast<double>(hits_ptr_->size()) / spatial_hash_.size();
}

void GraphClustering::constructGraph() {
  // This function is now renamed to processConnections and does streaming
  // Union-Find No edge storage - process connections immediately to save memory
  if (!hits_ptr_) {
    return;
  }

  // Initialize Union-Find for streaming processing
  initializeUnionFind();

  if (config_.enable_spatial_hash) {
    // Streaming approach: process connections immediately without storing edges
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, hits_ptr_->size()),
        [&](const tbb::blocked_range<size_t>& range) {
          size_t local_edge_count = 0;

          for (size_t hit_index = range.begin(); hit_index != range.end();
               ++hit_index) {
            const TDCHit& hit = (*hits_ptr_)[hit_index];
            auto neighbor_cells = getNeighborCells(hit.x, hit.y);

            // Process connections immediately - no storage
            for (const SpatialKey& cell_key : neighbor_cells) {
              auto cell_it = spatial_hash_.find(cell_key);
              if (cell_it == spatial_hash_.end()) {
                continue;
              }

              for (HitIndex neighbor_index : cell_it->second) {
                // Avoid duplicate processing and self-loops
                if (neighbor_index <= hit_index) {
                  continue;
                }

                const TDCHit& neighbor = (*hits_ptr_)[neighbor_index];
                if (shouldConnect(hit, neighbor)) {
                  // Process connection immediately with atomic Union-Find
                  atomicUnite(hit_index, neighbor_index);
                  local_edge_count++;
                }
              }
            }
          }

          // Track total connections processed (for statistics)
          edge_count_.fetch_add(local_edge_count, std::memory_order_relaxed);
        });

  } else {
    // Brute force streaming approach - no edge storage
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, hits_ptr_->size()),
        [&](const tbb::blocked_range<size_t>& range) {
          size_t local_edge_count = 0;

          for (size_t i = range.begin(); i != range.end(); ++i) {
            for (size_t j = i + 1; j < hits_ptr_->size(); ++j) {
              if (shouldConnect((*hits_ptr_)[i], (*hits_ptr_)[j])) {
                // Process connection immediately
                atomicUnite(i, j);
                local_edge_count++;
              }
            }
          }

          edge_count_.fetch_add(local_edge_count, std::memory_order_relaxed);
        });
  }

  stats_.total_edges = edge_count_.load();
}

void GraphClustering::initializeUnionFind() {
  if (!hits_ptr_) {
    return;
  }

  union_find_size_ = hits_ptr_->size();
  parent_ = std::make_unique<std::atomic<HitIndex>[]>(union_find_size_);
  rank_ = std::make_unique<std::atomic<int>[]>(union_find_size_);

  // Initialize Union-Find in parallel: each hit is its own parent
  tbb::parallel_for(tbb::blocked_range<size_t>(0, union_find_size_),
                    [&](const tbb::blocked_range<size_t>& range) {
                      for (size_t i = range.begin(); i != range.end(); ++i) {
                        parent_[i].store(i, std::memory_order_relaxed);
                        rank_[i].store(0, std::memory_order_relaxed);
                      }
                    });
}

GraphClustering::HitIndex GraphClustering::atomicFind(HitIndex x) {
  HitIndex parent = parent_[x].load(std::memory_order_acquire);

  // Path compression with atomic operations
  while (parent != x) {
    HitIndex grandparent = parent_[parent].load(std::memory_order_acquire);
    if (grandparent != parent) {
      // Try to compress path by updating parent directly
      parent_[x].compare_exchange_weak(parent, grandparent,
                                       std::memory_order_release);
    }
    x = parent;
    parent = grandparent;
  }

  return x;
}

void GraphClustering::atomicUnite(HitIndex x, HitIndex y) {
  // Find roots with atomic operations
  HitIndex root_x = atomicFind(x);
  HitIndex root_y = atomicFind(y);

  if (root_x == root_y) {
    return;  // Already in same set
  }

  // Union by rank with atomic operations
  int rank_x = rank_[root_x].load(std::memory_order_acquire);
  int rank_y = rank_[root_y].load(std::memory_order_acquire);

  if (rank_x < rank_y) {
    // Make root_y the parent of root_x
    parent_[root_x].store(root_y, std::memory_order_release);
  } else if (rank_x > rank_y) {
    // Make root_x the parent of root_y
    parent_[root_y].store(root_x, std::memory_order_release);
  } else {
    // Equal ranks: make root_x parent and increment its rank
    parent_[root_y].store(root_x, std::memory_order_release);
    rank_[root_x].fetch_add(1, std::memory_order_acq_rel);
  }
}

size_t GraphClustering::findConnectedComponents() {
  if (!hits_ptr_) {
    return 0;
  }

  // Union-Find has already been processed during streaming in constructGraph()
  // Now we just need to extract the final cluster assignments

  // Initialize cluster labels (all unclustered initially)
  cluster_labels_.assign(hits_ptr_->size(), -1);

  // Find final roots for all hits
  std::vector<HitIndex> roots(hits_ptr_->size());

  if (hits_ptr_->size() >= config_.parallel_threshold && parent_) {
    // Use parallel atomic Union-Find
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, hits_ptr_->size()),
        [&](const tbb::blocked_range<size_t>& range) {
          for (size_t hit_index = range.begin(); hit_index != range.end();
               ++hit_index) {
            // Use atomic find for final root resolution with bounds check
            if (hit_index < union_find_size_) {
              HitIndex current = hit_index;
              while (true) {
                HitIndex parent =
                    parent_[current].load(std::memory_order_acquire);
                if (parent == current) {
                  roots[hit_index] = current;
                  break;
                }
                current = parent;
              }
            } else {
              roots[hit_index] = hit_index;  // Fallback for out-of-bounds
            }
          }
        });
  } else {
    // Use sequential Union-Find
    for (size_t hit_index = 0; hit_index < hits_ptr_->size(); ++hit_index) {
      if (hit_index < union_find_size_) {
        roots[hit_index] = sequentialFind(hit_index);
      } else {
        roots[hit_index] = hit_index;  // Fallback for out-of-bounds
      }
    }
  }

  // Assign cluster IDs to connected components (sequential for correctness)
  std::unordered_map<HitIndex, int32_t> root_to_cluster;
  std::vector<size_t> cluster_sizes;
  int32_t cluster_id = 0;

  for (size_t hit_index = 0; hit_index < hits_ptr_->size(); ++hit_index) {
    HitIndex root = roots[hit_index];

    auto it = root_to_cluster.find(root);
    if (it == root_to_cluster.end()) {
      // New component found
      root_to_cluster[root] = cluster_id;
      cluster_sizes.push_back(1);
      cluster_labels_[hit_index] = cluster_id;
      cluster_id++;
    } else {
      // Existing component
      cluster_labels_[hit_index] = it->second;
      cluster_sizes[it->second]++;
    }
  }

  // Filter out clusters below minimum size (parallel filtering)
  tbb::parallel_for(
      tbb::blocked_range<size_t>(0, hits_ptr_->size()),
      [&](const tbb::blocked_range<size_t>& range) {
        for (size_t hit_index = range.begin(); hit_index != range.end();
             ++hit_index) {
          int current_cluster = cluster_labels_[hit_index];
          if (current_cluster >= 0 &&
              cluster_sizes[current_cluster] < config_.min_cluster_size) {
            cluster_labels_[hit_index] = -1;  // Mark as unclustered
          }
        }
      });

  // Count valid clusters after filtering
  std::unordered_set<int> valid_cluster_ids;
  for (size_t hit_index = 0; hit_index < hits_ptr_->size(); ++hit_index) {
    int current_cluster = cluster_labels_[hit_index];
    if (current_cluster >= 0) {
      valid_cluster_ids.insert(current_cluster);
    }
  }

  return valid_cluster_ids.size();
}

bool GraphClustering::shouldConnect(const TDCHit& hit1,
                                    const TDCHit& hit2) const {
  // Check temporal constraint first (cheaper)
  uint32_t tof_diff =
      (hit1.tof > hit2.tof) ? (hit1.tof - hit2.tof) : (hit2.tof - hit1.tof);
  uint32_t max_tof_diff =
      static_cast<uint32_t>(config_.neutron_correlation_window / 25.0);

  if (tof_diff > max_tof_diff) {
    return false;
  }

  // Check spatial constraint
  double dx = static_cast<double>(hit1.x) - static_cast<double>(hit2.x);
  double dy = static_cast<double>(hit1.y) - static_cast<double>(hit2.y);
  double distance = std::sqrt(dx * dx + dy * dy);

  return distance <= config_.radius;
}

void GraphClustering::updateStatistics() {
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
      end_time - start_time_);

  stats_.total_hits = hits_ptr_ ? hits_ptr_->size() : 0;
  stats_.processing_time_ms =
      duration.count() / 1000000.0;  // Convert nanoseconds to milliseconds

  // Count clusters and calculate statistics
  std::unordered_map<int, size_t> cluster_size_count;
  size_t unclustered = 0;

  for (int cluster_id : cluster_labels_) {
    if (cluster_id == -1) {
      unclustered++;
    } else {
      cluster_size_count[cluster_id]++;
    }
  }

  stats_.total_clusters = cluster_size_count.size();
  stats_.unclustered_hits = unclustered;

  // Calculate mean cluster size
  if (stats_.total_clusters > 0) {
    size_t total_clustered_hits = stats_.total_hits - unclustered;
    stats_.mean_cluster_size =
        static_cast<double>(total_clustered_hits) / stats_.total_clusters;
  } else {
    stats_.mean_cluster_size = 0.0;
  }
}

void GraphClustering::clearInternalState() {
  spatial_hash_.clear();
  cluster_labels_.clear();
  // Clear Union-Find structures
  parent_.reset();
  rank_.reset();
  seq_parent_.reset();
  seq_rank_.reset();
  union_find_size_ = 0;
  edge_count_.store(0);
  hits_ptr_ = nullptr;
}

// Sequential implementations for small datasets (no TBB overhead)

void GraphClustering::initializeUnionFindSequential() {
  if (!hits_ptr_) {
    return;
  }

  union_find_size_ = hits_ptr_->size();
  seq_parent_ = std::make_unique<HitIndex[]>(union_find_size_);
  seq_rank_ = std::make_unique<int[]>(union_find_size_);

  // Initialize: each hit is its own parent
  for (size_t i = 0; i < union_find_size_; ++i) {
    seq_parent_[i] = i;
    seq_rank_[i] = 0;
  }
}

GraphClustering::HitIndex GraphClustering::sequentialFind(HitIndex x) {
  // Path compression
  if (seq_parent_[x] != x) {
    seq_parent_[x] = sequentialFind(seq_parent_[x]);
  }
  return seq_parent_[x];
}

void GraphClustering::sequentialUnite(HitIndex x, HitIndex y) {
  HitIndex root_x = sequentialFind(x);
  HitIndex root_y = sequentialFind(y);

  if (root_x == root_y) {
    return;  // Already in same set
  }

  // Union by rank
  if (seq_rank_[root_x] < seq_rank_[root_y]) {
    seq_parent_[root_x] = root_y;
  } else if (seq_rank_[root_x] > seq_rank_[root_y]) {
    seq_parent_[root_y] = root_x;
  } else {
    seq_parent_[root_y] = root_x;
    seq_rank_[root_x]++;
  }
}

void GraphClustering::constructGraphSequential() {
  if (!hits_ptr_) {
    return;
  }

  // Initialize sequential Union-Find
  initializeUnionFindSequential();

  size_t edge_count = 0;

  if (config_.enable_spatial_hash) {
    // Sequential spatial hash approach
    for (size_t hit_index = 0; hit_index < hits_ptr_->size(); ++hit_index) {
      const TDCHit& hit = (*hits_ptr_)[hit_index];
      auto neighbor_cells = getNeighborCells(hit.x, hit.y);

      for (const SpatialKey& cell_key : neighbor_cells) {
        auto cell_it = spatial_hash_.find(cell_key);
        if (cell_it == spatial_hash_.end()) {
          continue;
        }

        for (HitIndex neighbor_index : cell_it->second) {
          // Avoid duplicate processing and self-loops
          if (neighbor_index <= hit_index) {
            continue;
          }

          const TDCHit& neighbor = (*hits_ptr_)[neighbor_index];
          if (shouldConnect(hit, neighbor)) {
            sequentialUnite(hit_index, neighbor_index);
            edge_count++;
          }
        }
      }
    }
  } else {
    // Brute force sequential approach
    for (size_t i = 0; i < hits_ptr_->size(); ++i) {
      for (size_t j = i + 1; j < hits_ptr_->size(); ++j) {
        if (shouldConnect((*hits_ptr_)[i], (*hits_ptr_)[j])) {
          sequentialUnite(i, j);
          edge_count++;
        }
      }
    }
  }

  edge_count_.store(edge_count);
  stats_.total_edges = edge_count;
}

// Temporal batching static functions

BatchStats GraphClustering::analyzeHitDistribution(
    const std::vector<TDCHit>& hits, int num_pulses,
    double correlation_window) {
  BatchStats stats;

  if (hits.empty()) {
    return stats;
  }

  // Constants for TOF analysis (25ns units)
  const uint32_t correlation_window_tof =
      static_cast<uint32_t>(correlation_window / 25.0);
  const uint32_t pulse_period_tof =
      static_cast<uint32_t>(16.667 * 1e6 / 25.0);  // 16.667ms in 25ns units

  // Phase 1: Detect pulse boundaries by finding TOF resets
  std::vector<size_t> pulse_boundaries;
  pulse_boundaries.push_back(0);  // First pulse starts at index 0

  uint32_t prev_tof = hits[0].tof;
  for (size_t i = 1; i < hits.size(); ++i) {
    uint32_t current_tof = hits[i].tof;

    // Detect TOF reset (significant decrease indicates new pulse)
    if (current_tof < prev_tof &&
        (prev_tof - current_tof) > (pulse_period_tof / 2)) {
      pulse_boundaries.push_back(i);

      // Stop if we have enough pulses
      if (pulse_boundaries.size() > static_cast<size_t>(num_pulses)) {
        break;
      }
    }
    prev_tof = current_tof;
  }

  // If we didn't find enough pulses, analyze what we have
  if (pulse_boundaries.size() < 2) {
    // Single pulse or no clear boundaries - use entire dataset
    pulse_boundaries.push_back(hits.size());
  }

  // Phase 2: Analyze hit distribution within correlation windows
  std::vector<size_t> window_hit_counts;

  for (size_t pulse_idx = 0; pulse_idx < pulse_boundaries.size() - 1;
       ++pulse_idx) {
    size_t pulse_start = pulse_boundaries[pulse_idx];
    size_t pulse_end = pulse_boundaries[pulse_idx + 1];

    // Sliding window analysis within this pulse
    for (size_t window_start = pulse_start; window_start < pulse_end;) {
      uint32_t window_start_tof = hits[window_start].tof;
      uint32_t window_end_tof = window_start_tof + correlation_window_tof;

      // Count hits in this correlation window
      size_t hit_count = 0;

      for (size_t i = window_start; i < pulse_end; ++i) {
        if (hits[i].tof <= window_end_tof) {
          hit_count++;
        } else {
          break;
        }
      }

      window_hit_counts.push_back(hit_count);

      // Move to next window (overlap by 50% for better statistics)
      window_start += std::max(size_t(1), hit_count / 2);
    }
  }

  // Phase 3: Calculate statistics
  if (!window_hit_counts.empty()) {
    // Calculate mean and standard deviation
    double sum = 0.0;
    for (size_t count : window_hit_counts) {
      sum += count;
    }
    stats.mean_hits_per_window = sum / window_hit_counts.size();

    // Calculate standard deviation
    double variance = 0.0;
    for (size_t count : window_hit_counts) {
      double diff = count - stats.mean_hits_per_window;
      variance += diff * diff;
    }
    stats.std_hits_per_window = std::sqrt(variance / window_hit_counts.size());

    // Set optimal window size (4x correlation window for batching)
    stats.optimal_window_tof = correlation_window_tof * 4;

    // Calculate 3σ overlap size for boundary handling
    stats.overlap_size = static_cast<size_t>(std::max(
        1.0, stats.mean_hits_per_window + 3.0 * stats.std_hits_per_window));
  }

  // Fill in remaining statistics
  stats.num_pulses_analyzed =
      std::min(pulse_boundaries.size() - 1, static_cast<size_t>(num_pulses));
  stats.pulse_period_tof = pulse_period_tof;
  stats.total_hits_analyzed =
      pulse_boundaries.size() > 1
          ? pulse_boundaries[std::min(pulse_boundaries.size() - 1,
                                      static_cast<size_t>(num_pulses))]
          : hits.size();

  return stats;
}

std::vector<HitBatch> GraphClustering::createStatisticalBatches(
    const std::vector<TDCHit>& hits, const BatchStats& stats) {
  std::vector<HitBatch> batches;

  if (hits.empty() || stats.mean_hits_per_window == 0.0) {
    return batches;
  }

  // Estimate hits per batch using statistical mean
  size_t estimated_hits_per_batch = static_cast<size_t>(
      stats.mean_hits_per_window * 4);  // 4x correlation window
  estimated_hits_per_batch =
      std::max(estimated_hits_per_batch, size_t(1000));  // Minimum batch size

  size_t current_start = 0;
  uint32_t current_tof_start = hits[0].tof;

  while (current_start < hits.size()) {
    HitBatch batch;
    batch.hits_ptr = &hits;
    batch.start_index = current_start;
    batch.tof_window_start = current_tof_start;

    // Find end of batch based on estimated size
    size_t estimated_end =
        std::min(current_start + estimated_hits_per_batch, hits.size());

    // Adjust end to respect TOF boundaries (avoid splitting tight temporal
    // clusters)
    size_t actual_end = estimated_end;
    if (estimated_end < hits.size()) {
      uint32_t target_tof = hits[estimated_end].tof;
      uint32_t correlation_window_tof =
          static_cast<uint32_t>(75.0 / 25.0);  // 75ns in 25ns units

      // Move end to a natural boundary (gap larger than correlation window)
      for (size_t i = estimated_end;
           i < std::min(estimated_end + stats.overlap_size, hits.size()); ++i) {
        if (hits[i].tof - target_tof > correlation_window_tof) {
          actual_end = i;
          break;
        }
      }
    }

    batch.end_index = actual_end;
    batch.tof_window_end =
        actual_end > 0 ? hits[actual_end - 1].tof : current_tof_start;

    // Add overlap for boundary handling
    batch.overlap_start = current_start;
    batch.overlap_end = std::min(actual_end + stats.overlap_size, hits.size());

    batches.push_back(batch);

    // Move to next batch with overlap
    current_start = actual_end;
    current_tof_start = actual_end < hits.size() ? hits[actual_end].tof : 0;
  }

  return batches;
}

std::vector<TDCNeutron> GraphClustering::processBatch(
    const HitBatch& batch, const GraphConfig& config) {
  std::vector<TDCNeutron> neutrons;

  if (!batch.isValid() || batch.size() == 0) {
    return neutrons;
  }

  // Create a temporary hit vector for this batch (copy for processing)
  std::vector<TDCHit> batch_hits;
  batch_hits.reserve(batch.size());

  const auto& hits = *batch.hits_ptr;
  for (size_t i = batch.start_index; i < batch.end_index; ++i) {
    batch_hits.push_back(hits[i]);
  }

  // Use existing graph clustering algorithm on batch
  GraphClustering clustering(config);
  clustering.fit(batch_hits);

  // Extract neutrons using centroid fitting
  CentroidConfig centroid_config;  // Use default VENUS settings
  CentroidPeakFitting peak_fitting(centroid_config);
  neutrons = peak_fitting.extractNeutrons(batch_hits);

  return neutrons;
}

// TemporalGraphClusteringProcessor implementation

TemporalGraphClusteringProcessor::TemporalGraphClusteringProcessor(
    const TemporalGraphConfig& config)
    : config_(config), stats_() {
  initializeWorkers();
}

TemporalGraphClusteringProcessor::~TemporalGraphClusteringProcessor() {
  workers_.clear();
  worker_results_.clear();
}

std::vector<TDCNeutron> TemporalGraphClusteringProcessor::processHits(
    const std::vector<TDCHit>& hits) {
  auto total_start = std::chrono::high_resolution_clock::now();

  // Reset statistics
  stats_ = ProcessingStats();
  stats_.total_hits_processed = hits.size();

  if (hits.empty()) {
    return std::vector<TDCNeutron>();
  }

  // Phase 1: Analyze hit distribution for statistical batching
  auto analysis_start = std::chrono::high_resolution_clock::now();
  auto batch_stats = GraphClustering::analyzeHitDistribution(
      hits, 2, config_.graph_config.neutron_correlation_window);
  auto analysis_end = std::chrono::high_resolution_clock::now();
  stats_.analysis_time_ms =
      std::chrono::duration_cast<std::chrono::microseconds>(analysis_end -
                                                            analysis_start)
          .count() /
      1000.0;

  // Phase 2: Create temporal batches with statistical sizing
  auto batching_start = std::chrono::high_resolution_clock::now();
  auto batches = GraphClustering::createStatisticalBatches(hits, batch_stats);
  auto batching_end = std::chrono::high_resolution_clock::now();
  stats_.batching_time_ms =
      std::chrono::duration_cast<std::chrono::microseconds>(batching_end -
                                                            batching_start)
          .count() /
      1000.0;
  stats_.num_batches_created = batches.size();

  if (batches.empty()) {
    return std::vector<TDCNeutron>();
  }

  // Phase 3: Process batches in parallel using worker pool
  auto processing_start = std::chrono::high_resolution_clock::now();
  auto neutrons = processBatchesParallel(batches);
  auto processing_end = std::chrono::high_resolution_clock::now();
  stats_.processing_time_ms =
      std::chrono::duration_cast<std::chrono::microseconds>(processing_end -
                                                            processing_start)
          .count() /
      1000.0;

  // Phase 4: Update final statistics
  auto total_end = std::chrono::high_resolution_clock::now();
  stats_.total_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(
                             total_end - total_start)
                             .count() /
                         1000.0;
  stats_.total_neutrons_produced = neutrons.size();
  stats_.hits_per_second =
      stats_.total_hits_processed / (stats_.total_time_ms / 1000.0);
  stats_.neutron_efficiency =
      static_cast<double>(stats_.total_neutrons_produced) /
      stats_.total_hits_processed;
  stats_.num_workers_used = workers_.size();

  return neutrons;
}

std::vector<TDCNeutron>
TemporalGraphClusteringProcessor::processBatchesParallel(
    const std::vector<HitBatch>& batches) {
  // Prepare worker results storage
  worker_results_.clear();
  worker_results_.resize(workers_.size());

  // Process batches in parallel using TBB
  tbb::parallel_for(
      tbb::blocked_range<size_t>(0, batches.size()),
      [&](const tbb::blocked_range<size_t>& range) {
        // Determine worker ID for this thread
        size_t worker_id =
            tbb::this_task_arena::current_thread_index() % workers_.size();

        for (size_t batch_idx = range.begin(); batch_idx != range.end();
             ++batch_idx) {
          const auto& batch = batches[batch_idx];

          // Process batch using assigned worker
          auto batch_neutrons =
              GraphClustering::processBatch(batch, config_.graph_config);

          // Accumulate results for this worker
          worker_results_[worker_id].insert(worker_results_[worker_id].end(),
                                            batch_neutrons.begin(),
                                            batch_neutrons.end());
        }
      });

  // Phase 4: Combine results from all workers with deduplication
  auto aggregation_start = std::chrono::high_resolution_clock::now();
  auto combined_neutrons = combineWorkerResults(worker_results_);
  auto aggregation_end = std::chrono::high_resolution_clock::now();
  stats_.aggregation_time_ms =
      std::chrono::duration_cast<std::chrono::microseconds>(aggregation_end -
                                                            aggregation_start)
          .count() /
      1000.0;

  return combined_neutrons;
}

std::vector<TDCNeutron> TemporalGraphClusteringProcessor::combineWorkerResults(
    const std::vector<std::vector<TDCNeutron>>& worker_results) {
  // Calculate total size for efficient allocation
  size_t total_neutrons = 0;
  for (const auto& worker_result : worker_results) {
    total_neutrons += worker_result.size();
  }

  if (total_neutrons == 0) {
    return std::vector<TDCNeutron>();
  }

  // Optimize for common case: single worker or no duplicates expected
  if (worker_results.size() == 1) {
    // Single worker - no deduplication needed, return copy
    return worker_results[0];
  }

  // Combine all worker results with pre-allocated capacity
  std::vector<TDCNeutron> combined_neutrons;
  combined_neutrons.reserve(total_neutrons);

  // Use move semantics when possible and efficient insertion
  for (const auto& worker_result : worker_results) {
    if (!worker_result.empty()) {
      combined_neutrons.insert(combined_neutrons.end(), worker_result.begin(),
                               worker_result.end());
    }
  }

  // Deduplicate neutrons from overlap regions if multiple workers produced
  // results
  if (combined_neutrons.size() > 1) {
    combined_neutrons = deduplicateNeutrons(combined_neutrons);
  }

  // Shrink to fit actual size after deduplication
  combined_neutrons.shrink_to_fit();

  return combined_neutrons;
}

std::vector<TDCNeutron> TemporalGraphClusteringProcessor::deduplicateNeutrons(
    std::vector<TDCNeutron>& neutrons) {
  if (neutrons.size() <= 1) {
    return neutrons;
  }

  // Sort neutrons by TOF for efficient temporal comparison
  std::sort(
      neutrons.begin(), neutrons.end(),
      [](const TDCNeutron& a, const TDCNeutron& b) { return a.tof < b.tof; });

  std::vector<TDCNeutron> deduplicated;
  deduplicated.reserve(neutrons.size());

  // Deduplication parameters (from overlap factor and graph config)
  const double spatial_tolerance =
      config_.graph_config.radius * 0.5;  // Half of clustering radius
  const uint32_t temporal_tolerance =
      static_cast<uint32_t>(config_.graph_config.neutron_correlation_window /
                            25.0);  // 75ns in 25ns units

  // Track which neutrons have been marked as duplicates
  std::vector<bool> is_duplicate(neutrons.size(), false);

  // Compare each neutron with subsequent neutrons within temporal window
  for (size_t i = 0; i < neutrons.size(); ++i) {
    if (is_duplicate[i]) {
      continue;  // Skip neutrons already marked as duplicates
    }

    const TDCNeutron& neutron_i = neutrons[i];

    // Look for duplicates in subsequent neutrons (within temporal tolerance)
    for (size_t j = i + 1; j < neutrons.size(); ++j) {
      const TDCNeutron& neutron_j = neutrons[j];

      // Stop searching if TOF difference exceeds temporal tolerance
      if (neutron_j.tof - neutron_i.tof > temporal_tolerance) {
        break;
      }

      if (is_duplicate[j]) {
        continue;  // Skip neutrons already marked as duplicates
      }

      // Calculate spatial distance
      double dx =
          static_cast<double>(neutron_i.x) - static_cast<double>(neutron_j.x);
      double dy =
          static_cast<double>(neutron_i.y) - static_cast<double>(neutron_j.y);
      double spatial_distance = std::sqrt(dx * dx + dy * dy);

      // Check if neutrons are duplicates (same spatial-temporal signature)
      if (spatial_distance <= spatial_tolerance) {
        // Found a duplicate - keep the one with more hits (higher confidence)

        if (neutron_i.n_hits >= neutron_j.n_hits) {
          // Keep neutron_i, mark neutron_j as duplicate
          is_duplicate[j] = true;
        } else {
          // Keep neutron_j, mark neutron_i as duplicate
          is_duplicate[i] = true;
          break;  // Stop checking for neutron_i
        }
      }
    }

    // Add neutron_i to deduplicated list if it's not a duplicate
    if (!is_duplicate[i]) {
      deduplicated.push_back(neutron_i);
    }
  }

  return deduplicated;
}

void TemporalGraphClusteringProcessor::initializeWorkers() {
  // Calculate optimal worker count
  size_t num_workers = config_.num_workers;
  if (num_workers == 0) {
    num_workers = calculateOptimalWorkerCount();
  }

  // Create worker instances
  workers_.clear();
  workers_.reserve(num_workers);

  for (size_t i = 0; i < num_workers; ++i) {
    workers_.push_back(std::make_unique<GraphClustering>(config_.graph_config));
  }

  // Initialize worker results storage
  worker_results_.clear();
  worker_results_.resize(num_workers);
}

size_t TemporalGraphClusteringProcessor::calculateOptimalWorkerCount() const {
  // Use hardware concurrency with some reasonable limits
  size_t hardware_threads = std::thread::hardware_concurrency();

  if (hardware_threads == 0) {
    hardware_threads =
        4;  // Fallback for systems that don't report thread count
  }

  // Use all available threads for high-performance clustering
  return hardware_threads;
}

const TemporalGraphClusteringProcessor::ProcessingStats&
TemporalGraphClusteringProcessor::getStatistics() const {
  return stats_;
}

void TemporalGraphClusteringProcessor::updateConfig(
    const TemporalGraphConfig& config) {
  config_ = config;
  initializeWorkers();  // Reinitialize workers with new config
}

const TemporalGraphConfig& TemporalGraphClusteringProcessor::getConfig() const {
  return config_;
}

void TemporalGraphClusteringProcessor::reset() {
  stats_ = ProcessingStats();

  // Clear worker results
  for (auto& worker_result : worker_results_) {
    worker_result.clear();
  }

  // Reset each worker
  for (auto& worker : workers_) {
    worker->reset();
  }
}

}  // namespace tdcsophiread