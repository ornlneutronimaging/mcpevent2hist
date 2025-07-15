// TDCSophiread Clustering State Implementation
// State management for stateless clustering algorithms

#include "neutron_processing/clustering_state.h"

#include <stdexcept>

#include "neutron_processing/dbscan_clustering.h"  // For DBSCANClusteringState
#include "neutron_processing/graph_clustering.h"   // For GraphClusteringState
#include "neutron_processing/grid_clustering.h"    // For GridClusteringState

namespace tdcsophiread {

// ABSClusteringState implementation

ABSClusteringState::ABSClusteringState(size_t initial_bucket_capacity) {
  // Pre-allocate bucket pool to avoid reallocations
  bucket_pool.reserve(initial_bucket_capacity);

  // Pre-allocate index vectors
  active_bucket_indices.reserve(initial_bucket_capacity / 2);
  free_bucket_indices.reserve(initial_bucket_capacity);

  // Initialize free bucket list
  for (size_t i = 0; i < initial_bucket_capacity; ++i) {
    bucket_pool.emplace_back();
    free_bucket_indices.push_back(i);
  }

  // Pre-allocate temporary vectors
  buckets_to_close.reserve(100);
  remaining_buckets.reserve(100);

  // Initialize spatial bins (already default constructed)
  reset();
}

void ABSClusteringState::reset() {
  // Reset active and free bucket lists
  active_bucket_indices.clear();
  free_bucket_indices.clear();

  // All buckets become free
  for (size_t i = 0; i < bucket_pool.size(); ++i) {
    bucket_pool[i].reset();
    free_bucket_indices.push_back(i);
  }

  // Clear spatial index
  for (auto& row : spatial_bins) {
    for (auto& bin : row) {
      bin.clear();
    }
  }

  // Clear temporary vectors (keep capacity)
  buckets_to_close.clear();
  remaining_buckets.clear();

  // Reset processing state
  next_cluster_id = 0;
  hits_processed = 0;
}

// ClusteringStateFactory implementation

std::unique_ptr<IClusteringState> ClusteringStateFactory::create(
    const std::string& algorithm_name) {
  if (algorithm_name == "abs") {
    return std::make_unique<ABSClusteringState>();
  } else if (algorithm_name == "graph") {
    return std::make_unique<GraphClusteringState>();
  } else if (algorithm_name == "dbscan") {
    return std::make_unique<DBSCANClusteringState>();
  } else if (algorithm_name == "grid") {
    return std::make_unique<GridClusteringState>();
  }

  throw std::invalid_argument("Unknown clustering algorithm: " +
                              algorithm_name);
}

bool ClusteringStateFactory::isSupported(const std::string& algorithm_name) {
  return algorithm_name == "abs" || algorithm_name == "graph" ||
         algorithm_name == "dbscan" || algorithm_name == "grid";
}

}  // namespace tdcsophiread