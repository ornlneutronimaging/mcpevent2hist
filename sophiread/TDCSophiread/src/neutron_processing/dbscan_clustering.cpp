// TDCSophiread DBSCAN Clustering Implementation
// Density-based clustering for temporal window processing

#include "neutron_processing/dbscan_clustering.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace tdcsophiread {

// DBSCANHitClustering Implementation

DBSCANHitClustering::DBSCANHitClustering() : config_{} {
  config_.algorithm = "dbscan";
  config_.dbscan.epsilon = 5.0;
  config_.dbscan.min_points = 4;
  config_.dbscan.neutron_correlation_window = 75.0;
  config_.dbscan.grid_size = 5.0;
}

DBSCANHitClustering::DBSCANHitClustering(const HitClusteringConfig& config)
    : config_(config) {
  config_.algorithm = "dbscan";
}

void DBSCANHitClustering::configure(const HitClusteringConfig& config) {
  config_ = config;
  config_.algorithm = "dbscan";
}

std::unique_ptr<IClusteringState> DBSCANHitClustering::createState() const {
  return std::make_unique<DBSCANClusteringState>();
}

size_t DBSCANHitClustering::cluster(std::vector<TDCHit>::const_iterator begin,
                                    std::vector<TDCHit>::const_iterator end,
                                    IClusteringState& state,
                                    std::vector<int>& cluster_labels) const {
  // Cast to DBSCAN-specific state
  auto& dbscan_state = static_cast<DBSCANClusteringState&>(state);

  // Reset state for new batch
  dbscan_state.reset();

  // Get number of hits
  size_t num_hits = std::distance(begin, end);
  if (num_hits == 0) {
    return 0;
  }

  // Initialize data structures
  cluster_labels.clear();
  cluster_labels.resize(num_hits, -1);  // Initialize all as noise
  dbscan_state.point_types.resize(num_hits,
                                  DBSCANClusteringState::PointType::UNDEFINED);
  dbscan_state.visited.resize(num_hits, false);

  // Build spatial index for efficient neighbor queries
  buildSpatialIndex(begin, end, dbscan_state);

  // DBSCAN main algorithm
  int cluster_id = 0;

  for (size_t i = 0; i < num_hits; ++i) {
    // Skip if already processed
    if (dbscan_state.visited[i]) {
      continue;
    }

    dbscan_state.visited[i] = true;

    // Find neighbors
    auto neighbors = findNeighbors(i, begin, end, dbscan_state);

    if (neighbors.size() < config_.dbscan.min_points) {
      // Mark as noise (for now - might become border point later)
      cluster_labels[i] = -1;
      dbscan_state.point_types[i] = DBSCANClusteringState::PointType::NOISE;
      dbscan_state.noise_points++;
    } else {
      // Core point - expand cluster
      dbscan_state.point_types[i] = DBSCANClusteringState::PointType::CORE;
      dbscan_state.core_points++;
      expandCluster(i, cluster_id, begin, end, dbscan_state, cluster_labels);
      cluster_id++;
    }
  }

  // Update statistics
  dbscan_state.hits_processed = num_hits;
  dbscan_state.clusters_found = cluster_id;

  return cluster_id;
}

void DBSCANHitClustering::expandCluster(
    size_t core_idx, int cluster_id, std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end, DBSCANClusteringState& state,
    std::vector<int>& cluster_labels) const {
  // Use queue for breadth-first expansion
  std::queue<size_t> seeds;

  // Add core point to cluster
  cluster_labels[core_idx] = cluster_id;

  // Get initial neighbors
  auto neighbors = findNeighbors(core_idx, begin, end, state);

  // Add neighbors to seeds
  for (size_t neighbor_idx : neighbors) {
    if (cluster_labels[neighbor_idx] == -1) {
      // Convert noise to border
      cluster_labels[neighbor_idx] = cluster_id;
      if (state.point_types[neighbor_idx] ==
          DBSCANClusteringState::PointType::NOISE) {
        state.point_types[neighbor_idx] =
            DBSCANClusteringState::PointType::BORDER;
        state.noise_points--;
        state.border_points++;
      }
    }

    if (!state.visited[neighbor_idx]) {
      seeds.push(neighbor_idx);
      state.visited[neighbor_idx] = true;
    }
  }

  // Expand cluster
  while (!seeds.empty()) {
    size_t current_idx = seeds.front();
    seeds.pop();

    auto current_neighbors = findNeighbors(current_idx, begin, end, state);

    if (current_neighbors.size() >= config_.dbscan.min_points) {
      // Current point is also a core point
      if (state.point_types[current_idx] !=
          DBSCANClusteringState::PointType::CORE) {
        auto prev_type = state.point_types[current_idx];
        state.point_types[current_idx] = DBSCANClusteringState::PointType::CORE;
        state.core_points++;
        if (prev_type == DBSCANClusteringState::PointType::BORDER) {
          state.border_points--;
        }
      }

      // Add unvisited neighbors to cluster
      for (size_t neighbor_idx : current_neighbors) {
        if (cluster_labels[neighbor_idx] < 0) {
          // Add to cluster
          cluster_labels[neighbor_idx] = cluster_id;

          if (state.point_types[neighbor_idx] ==
              DBSCANClusteringState::PointType::NOISE) {
            state.point_types[neighbor_idx] =
                DBSCANClusteringState::PointType::BORDER;
            state.noise_points--;
            state.border_points++;
          }
        }

        if (!state.visited[neighbor_idx]) {
          seeds.push(neighbor_idx);
          state.visited[neighbor_idx] = true;
        }
      }
    } else if (state.point_types[current_idx] ==
               DBSCANClusteringState::PointType::UNDEFINED) {
      // Border point
      state.point_types[current_idx] = DBSCANClusteringState::PointType::BORDER;
      state.border_points++;
    }
  }
}

std::vector<size_t> DBSCANHitClustering::findNeighbors(
    size_t hit_idx, std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end,
    const DBSCANClusteringState& state) const {
  std::vector<size_t> neighbors;

  // Validate hit_idx is within range
  size_t num_hits = std::distance(begin, end);
  if (hit_idx >= num_hits) {
    return neighbors;  // Return empty if out of bounds
  }

  const auto& hit = *(begin + hit_idx);

  // Get spatial cell
  size_t cell = getSpatialCell(hit.x, hit.y);

  // Check 3x3 neighborhood of cells
  int cell_x = cell % SPATIAL_GRID_SIZE;
  int cell_y = cell / SPATIAL_GRID_SIZE;

  // Calculate search radius in cells
  double cell_width = 512.0 / SPATIAL_GRID_SIZE;  // Assuming 512 detector size
  int cell_radius =
      static_cast<int>(std::ceil(config_.dbscan.epsilon / cell_width));

  for (int dy = -cell_radius; dy <= cell_radius; ++dy) {
    for (int dx = -cell_radius; dx <= cell_radius; ++dx) {
      int nx = cell_x + dx;
      int ny = cell_y + dy;

      // Skip out-of-bounds cells
      if (nx < 0 || nx >= static_cast<int>(SPATIAL_GRID_SIZE) || ny < 0 ||
          ny >= static_cast<int>(SPATIAL_GRID_SIZE)) {
        continue;
      }

      size_t neighbor_cell = ny * SPATIAL_GRID_SIZE + nx;
      const auto& cell_hits = state.spatial_index.getCell(neighbor_cell);

      // Check each hit in the cell
      for (size_t j : cell_hits) {
        // Ensure j is within valid range
        if (j >= num_hits) {
          continue;  // Skip invalid indices
        }

        // In DBSCAN, a point is its own neighbor
        if (j == hit_idx) {
          neighbors.push_back(j);
          continue;
        }

        const auto& other_hit = *(begin + j);

        // Check spatial and temporal constraints
        bool within_eps = withinEpsilon(hit, other_hit);
        bool within_time = withinTemporalWindow(hit, other_hit);

        if (within_eps && within_time) {
          neighbors.push_back(j);
        }
      }
    }
  }

  return neighbors;
}

void DBSCANHitClustering::buildSpatialIndex(
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end,
    DBSCANClusteringState& state) const {
  state.spatial_index.clear();

  size_t num_hits = std::distance(begin, end);
  for (size_t i = 0; i < num_hits; ++i) {
    const auto& hit = *(begin + i);
    size_t cell = getSpatialCell(hit.x, hit.y);
    state.spatial_index.insert(i, cell);
  }
}

size_t DBSCANHitClustering::getSpatialCell(double x, double y) const {
  // Map coordinates to grid cell
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

bool DBSCANHitClustering::withinEpsilon(const TDCHit& hit1,
                                        const TDCHit& hit2) const {
  double dx = hit1.x - hit2.x;
  double dy = hit1.y - hit2.y;
  double dist_sq = dx * dx + dy * dy;
  double epsilon = config_.dbscan.epsilon;
  return dist_sq <= epsilon * epsilon;
}

bool DBSCANHitClustering::withinTemporalWindow(const TDCHit& hit1,
                                               const TDCHit& hit2) const {
  uint32_t tof1 = hit1.tof;
  uint32_t tof2 = hit2.tof;

  // Calculate time difference
  uint32_t diff = (tof1 > tof2) ? (tof1 - tof2) : (tof2 - tof1);

  // Convert correlation window from ns to 25ns units
  uint32_t window_tof =
      static_cast<uint32_t>(config_.dbscan.neutron_correlation_window / 25.0);

  return diff <= window_tof;
}

ClusteringStatistics DBSCANHitClustering::getStatistics(
    const IClusteringState& state, size_t num_hits) const {
  const auto& dbscan_state = static_cast<const DBSCANClusteringState&>(state);

  ClusteringStatistics stats;
  stats.total_clusters = dbscan_state.clusters_found;
  stats.unclustered_hits = dbscan_state.noise_points;
  stats.mean_cluster_size =
      (dbscan_state.clusters_found > 0)
          ? static_cast<double>(num_hits - dbscan_state.noise_points) /
                dbscan_state.clusters_found
          : 0.0;
  stats.max_cluster_size = 0;  // Would need to track this separately
  stats.total_hits_processed = num_hits;

  // DBSCAN-specific statistics could be added:
  // - Number of core points
  // - Number of border points
  // - Density distribution

  return stats;
}

// DBSCANClusteringState Implementation

void DBSCANClusteringState::SpatialIndex::clear() {
  for (auto& cell : cells) {
    cell.clear();
  }
}

void DBSCANClusteringState::SpatialIndex::insert(size_t hit_idx, size_t cell) {
  if (cell < cells.size()) {
    cells[cell].push_back(hit_idx);
  }
}

const std::vector<size_t>& DBSCANClusteringState::SpatialIndex::getCell(
    size_t cell) const {
  static const std::vector<size_t> empty;
  return (cell < cells.size()) ? cells[cell] : empty;
}

DBSCANClusteringState::DBSCANClusteringState() { reset(); }

void DBSCANClusteringState::reset() {
  spatial_index.clear();
  point_types.clear();
  visited.clear();
  neighbor_buffer.clear();

  hits_processed = 0;
  clusters_found = 0;
  core_points = 0;
  border_points = 0;
  noise_points = 0;
}

}  // namespace tdcsophiread