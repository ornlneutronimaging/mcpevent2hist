// TDCSophiread Graph Clustering Implementation
// Graph-based clustering for temporal window processing

#include "neutron_processing/graph_clustering.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace tdcsophiread {

// GraphHitClustering Implementation

GraphHitClustering::GraphHitClustering() : config_{} {
  // Set graph-specific defaults
  config_.algorithm = "graph";
  // Set graph-specific defaults
  config_.graph.radius = DEFAULT_RADIUS;
  config_.graph.min_cluster_size = 1;
  // Use ABS defaults for temporal window
  config_.abs.neutron_correlation_window = 3000;  // 75ns in 25ns units
}

GraphHitClustering::GraphHitClustering(const HitClusteringConfig& config)
    : config_(config) {
  config_.algorithm = "graph";  // Ensure algorithm name is correct
}

void GraphHitClustering::configure(const HitClusteringConfig& config) {
  config_ = config;
  config_.algorithm = "graph";
}

std::unique_ptr<IClusteringState> GraphHitClustering::createState() const {
  return std::make_unique<GraphClusteringState>();
}

size_t GraphHitClustering::cluster(std::vector<TDCHit>::const_iterator begin,
                                   std::vector<TDCHit>::const_iterator end,
                                   IClusteringState& state,
                                   std::vector<int>& cluster_labels) const {
  // Cast to graph-specific state
  auto& graph_state = static_cast<GraphClusteringState&>(state);

  // Reset state for new batch
  graph_state.reset();

  // Get number of hits
  size_t num_hits = std::distance(begin, end);
  if (num_hits == 0) {
    return 0;
  }

  // Initialize Union-Find for all hits
  graph_state.union_find.init(num_hits);

  // Build spatial index and graph edges
  buildGraph(begin, end, graph_state);

  // Find connected components
  size_t num_clusters = findConnectedComponents(graph_state, cluster_labels);

  // Update statistics
  graph_state.hits_processed = num_hits;
  graph_state.clusters_found = num_clusters;

  return num_clusters;
}

void GraphHitClustering::buildGraph(std::vector<TDCHit>::const_iterator begin,
                                    std::vector<TDCHit>::const_iterator end,
                                    GraphClusteringState& state) const {
  size_t num_hits = std::distance(begin, end);

  // Clear spatial index
  state.spatial_index.clear();

  // Build spatial index
  for (size_t i = 0; i < num_hits; ++i) {
    const auto& hit = *(begin + i);
    size_t cell = getSpatialCell(hit.x, hit.y);
    state.spatial_index.insert(i, cell);
  }

  // For each hit, check neighbors in adjacent cells
  for (size_t i = 0; i < num_hits; ++i) {
    const auto& hit1 = *(begin + i);
    size_t cell = getSpatialCell(hit1.x, hit1.y);

    // Check 3x3 neighborhood of cells
    int cell_x = cell % SPATIAL_GRID_SIZE;
    int cell_y = cell / SPATIAL_GRID_SIZE;

    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        int nx = cell_x + dx;
        int ny = cell_y + dy;

        // Skip out-of-bounds cells
        if (nx < 0 || nx >= static_cast<int>(SPATIAL_GRID_SIZE) || ny < 0 ||
            ny >= static_cast<int>(SPATIAL_GRID_SIZE)) {
          continue;
        }

        size_t neighbor_cell = ny * SPATIAL_GRID_SIZE + nx;
        const auto& neighbors = state.spatial_index.getCell(neighbor_cell);

        // Check each neighbor in the cell
        for (size_t j : neighbors) {
          // Only check forward connections to avoid duplicates
          if (j <= i) continue;

          const auto& hit2 = *(begin + j);

          // Check spatial and temporal constraints
          if (withinRadius(hit1, hit2) && withinTemporalWindow(hit1, hit2)) {
            // Create edge by uniting in Union-Find
            state.union_find.unite(static_cast<int>(i), static_cast<int>(j));
            state.edges_created++;
          }
        }
      }
    }
  }
}

size_t GraphHitClustering::findConnectedComponents(
    GraphClusteringState& state, std::vector<int>& cluster_labels) const {
  size_t num_hits = cluster_labels.size();

  // Map from root to cluster ID
  std::vector<int> root_to_cluster(num_hits, -1);
  int next_cluster_id = 0;

  // Assign cluster IDs based on connected components
  for (size_t i = 0; i < num_hits; ++i) {
    int root = state.union_find.find(static_cast<int>(i));

    if (root_to_cluster[root] == -1) {
      root_to_cluster[root] = next_cluster_id++;
    }

    cluster_labels[i] = root_to_cluster[root];
  }

  // Filter out small clusters if configured
  if (config_.graph.min_cluster_size > 1) {
    std::vector<int> cluster_sizes(next_cluster_id, 0);

    // Count cluster sizes
    for (int label : cluster_labels) {
      if (label >= 0) {
        cluster_sizes[label]++;
      }
    }

    // Mark small clusters as noise
    for (size_t i = 0; i < num_hits; ++i) {
      if (cluster_labels[i] >= 0 &&
          cluster_sizes[cluster_labels[i]] <
              static_cast<int>(config_.graph.min_cluster_size)) {
        cluster_labels[i] = -1;  // Mark as unclustered
      }
    }

    // Count remaining clusters
    std::vector<bool> cluster_exists(next_cluster_id, false);
    for (int label : cluster_labels) {
      if (label >= 0) {
        cluster_exists[label] = true;
      }
    }

    return std::count(cluster_exists.begin(), cluster_exists.end(), true);
  }

  return next_cluster_id;
}

size_t GraphHitClustering::getSpatialCell(double x, double y) const {
  // Map coordinates to grid cell
  // Assuming coordinates are in range [0, 512] for VENUS detector
  const double DETECTOR_SIZE = 512.0;

  int cell_x = static_cast<int>(x / DETECTOR_SIZE * SPATIAL_GRID_SIZE);
  int cell_y = static_cast<int>(y / DETECTOR_SIZE * SPATIAL_GRID_SIZE);

  // Clamp to grid bounds
  cell_x =
      std::max(0, std::min(static_cast<int>(SPATIAL_GRID_SIZE - 1), cell_x));
  cell_y =
      std::max(0, std::min(static_cast<int>(SPATIAL_GRID_SIZE - 1), cell_y));

  return cell_y * SPATIAL_GRID_SIZE + cell_x;
}

bool GraphHitClustering::withinRadius(const TDCHit& hit1,
                                      const TDCHit& hit2) const {
  double dx = hit1.x - hit2.x;
  double dy = hit1.y - hit2.y;
  double dist_sq = dx * dx + dy * dy;
  double radius = config_.graph.radius;
  return dist_sq <= radius * radius;
}

bool GraphHitClustering::withinTemporalWindow(const TDCHit& hit1,
                                              const TDCHit& hit2) const {
  // Use getTOF() for temporal comparison (in 25ns units)
  uint32_t tof1 = hit1.tof;
  uint32_t tof2 = hit2.tof;

  // Calculate time difference considering TOF wraparound
  uint32_t diff = (tof1 > tof2) ? (tof1 - tof2) : (tof2 - tof1);

  // Check against correlation window (in 25ns units)
  // Use temporal window from ABS config since graph doesn't have its own
  return diff <= config_.abs.neutron_correlation_window;
}

ClusteringStatistics GraphHitClustering::getStatistics(
    const IClusteringState& state, size_t num_hits) const {
  const auto& graph_state = static_cast<const GraphClusteringState&>(state);

  ClusteringStatistics stats;
  stats.total_clusters = graph_state.clusters_found;
  stats.unclustered_hits = 0;  // Graph clustering doesn't have unclustered hits
  stats.mean_cluster_size =
      (graph_state.clusters_found > 0)
          ? static_cast<double>(num_hits) / graph_state.clusters_found
          : 0.0;
  stats.max_cluster_size = 0;  // Would need to track this separately
  stats.total_hits_processed = num_hits;

  return stats;
}

// GraphClusteringState Implementation

void GraphClusteringState::UnionFind::init(size_t n) {
  parent.resize(n);
  rank.resize(n, 0);
  std::iota(parent.begin(), parent.end(), 0);  // Each node is its own parent
}

int GraphClusteringState::UnionFind::find(int x) {
  if (parent[x] != x) {
    parent[x] = find(parent[x]);  // Path compression
  }
  return parent[x];
}

void GraphClusteringState::UnionFind::unite(int x, int y) {
  int root_x = find(x);
  int root_y = find(y);

  if (root_x == root_y) return;

  // Union by rank
  if (rank[root_x] < rank[root_y]) {
    parent[root_x] = root_y;
  } else if (rank[root_x] > rank[root_y]) {
    parent[root_y] = root_x;
  } else {
    parent[root_y] = root_x;
    rank[root_x]++;
  }
}

void GraphClusteringState::UnionFind::reset() {
  parent.clear();
  rank.clear();
}

void GraphClusteringState::SpatialIndex::clear() {
  for (auto& cell : cells) {
    cell.clear();
  }
}

void GraphClusteringState::SpatialIndex::insert(size_t hit_idx, size_t cell) {
  if (cell < cells.size()) {
    cells[cell].push_back(hit_idx);
  }
}

const std::vector<size_t>& GraphClusteringState::SpatialIndex::getCell(
    size_t cell) const {
  static const std::vector<size_t> empty;
  return (cell < cells.size()) ? cells[cell] : empty;
}

GraphClusteringState::GraphClusteringState() { reset(); }

void GraphClusteringState::reset() {
  union_find.reset();
  spatial_index.clear();
  edges_created = 0;
  hits_processed = 0;
  clusters_found = 0;
}

}  // namespace tdcsophiread