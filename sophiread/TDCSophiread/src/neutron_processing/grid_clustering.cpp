// TDCSophiread Grid-Based Clustering Implementation
// Fast O(n) clustering using detector grid structure

#include "neutron_processing/grid_clustering.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace tdcsophiread {

// GridHitClustering Implementation

GridHitClustering::GridHitClustering() : config_{} {
  config_.algorithm = "grid";
  // Grid defaults are set in GridConfig constructor
}

GridHitClustering::GridHitClustering(const HitClusteringConfig& config)
    : config_(config) {
  config_.algorithm = "grid";
}

void GridHitClustering::configure(const HitClusteringConfig& config) {
  config_ = config;
  config_.algorithm = "grid";
}

std::unique_ptr<IClusteringState> GridHitClustering::createState() const {
  auto state = std::make_unique<GridClusteringState>();
  state->initializeGrid(getGridCols(), getGridRows());
  return state;
}

size_t GridHitClustering::getGridCols() const { return config_.grid.grid_cols; }

size_t GridHitClustering::getGridRows() const { return config_.grid.grid_rows; }

size_t GridHitClustering::cluster(std::vector<TDCHit>::const_iterator begin,
                                  std::vector<TDCHit>::const_iterator end,
                                  IClusteringState& state,
                                  std::vector<int>& cluster_labels) const {
  // Cast to grid-specific state
  auto& grid_state = static_cast<GridClusteringState&>(state);

  // Reset state for new batch
  grid_state.reset();
  grid_state.initializeGrid(getGridCols(), getGridRows());

  // Get number of hits
  size_t num_hits = std::distance(begin, end);
  if (num_hits == 0) {
    return 0;
  }

  // Initialize cluster labels
  cluster_labels.clear();
  cluster_labels.resize(num_hits, -1);

  // Phase 1: Assign hits to grid cells - O(n)
  for (size_t i = 0; i < num_hits; ++i) {
    const auto& hit = *(begin + i);
    size_t cell = getGridCell(hit.x, hit.y, getGridCols(), getGridRows());
    grid_state.insertHit(i, cell);
  }

  // Update statistics
  grid_state.hits_processed = num_hits;

  // Phase 2: Process each grid cell - O(n) total
  int next_cluster_id = 0;

  for (size_t cell_idx = 0; cell_idx < grid_state.getNumCells(); ++cell_idx) {
    const auto& cell = grid_state.getCell(cell_idx);
    if (cell.hit_indices.empty()) {
      continue;
    }

    grid_state.cells_with_hits++;
    grid_state.max_hits_per_cell =
        std::max(grid_state.max_hits_per_cell, cell.hit_indices.size());

    // Process hits within this cell
    processGridCell(cell_idx, begin, grid_state, cluster_labels,
                    next_cluster_id);
  }

  // Phase 3: Merge clusters across cell boundaries if enabled
  if (config_.grid.merge_adjacent_cells) {
    // Use Union-Find to merge clusters that span cell boundaries
    std::unordered_map<int, int> cluster_parent;

    // Initialize each cluster as its own parent
    for (int i = 0; i < next_cluster_id; ++i) {
      cluster_parent[i] = i;
    }

    // Find function with path compression
    std::function<int(int)> find = [&](int x) {
      if (cluster_parent[x] != x) {
        cluster_parent[x] = find(cluster_parent[x]);
      }
      return cluster_parent[x];
    };

    // Union function
    auto unite = [&](int x, int y) {
      int px = find(x);
      int py = find(y);
      if (px != py) {
        cluster_parent[px] = py;
      }
    };

    // Check adjacent cells for merging
    size_t cols = getGridCols();
    size_t rows = getGridRows();

    for (size_t row = 0; row < rows; ++row) {
      for (size_t col = 0; col < cols; ++col) {
        size_t cell_idx = row * cols + col;
        const auto& cell = grid_state.getCell(cell_idx);
        if (cell.hit_indices.empty()) continue;

        // Check right neighbor
        if (col + 1 < cols) {
          size_t right_cell = row * cols + (col + 1);
          const auto& right = grid_state.getCell(right_cell);
          if (!right.hit_indices.empty()) {
            // Check if any hits should be connected across boundary
            for (size_t i : cell.hit_indices) {
              if (cluster_labels[i] < 0) continue;
              const auto& hit1 = *(begin + i);

              for (size_t j : right.hit_indices) {
                if (cluster_labels[j] < 0) continue;
                if (cluster_labels[i] == cluster_labels[j]) continue;

                const auto& hit2 = *(begin + j);
                if (areHitsConnected(hit1, hit2)) {
                  unite(cluster_labels[i], cluster_labels[j]);
                }
              }
            }
          }
        }

        // Check bottom neighbor
        if (row + 1 < rows) {
          size_t bottom_cell = (row + 1) * cols + col;
          const auto& bottom = grid_state.getCell(bottom_cell);
          if (!bottom.hit_indices.empty()) {
            // Check if any hits should be connected across boundary
            for (size_t i : cell.hit_indices) {
              if (cluster_labels[i] < 0) continue;
              const auto& hit1 = *(begin + i);

              for (size_t j : bottom.hit_indices) {
                if (cluster_labels[j] < 0) continue;
                if (cluster_labels[i] == cluster_labels[j]) continue;

                const auto& hit2 = *(begin + j);
                if (areHitsConnected(hit1, hit2)) {
                  unite(cluster_labels[i], cluster_labels[j]);
                }
              }
            }
          }
        }
      }
    }

    // Relabel clusters based on Union-Find results
    std::unordered_map<int, int> new_labels;
    int final_cluster_count = 0;

    for (size_t i = 0; i < num_hits; ++i) {
      if (cluster_labels[i] >= 0) {
        int root = find(cluster_labels[i]);
        if (new_labels.find(root) == new_labels.end()) {
          new_labels[root] = final_cluster_count++;
        }
        cluster_labels[i] = new_labels[root];
      }
    }

    grid_state.clusters_found = final_cluster_count;
    return final_cluster_count;
  }

  grid_state.clusters_found = next_cluster_id;
  return next_cluster_id;
}

size_t GridHitClustering::getGridCell(double x, double y, size_t cols,
                                      size_t rows) const {
  const double DETECTOR_SIZE = 512.0;

  // Map coordinates to grid cell
  int cell_x = static_cast<int>(x / DETECTOR_SIZE * cols);
  int cell_y = static_cast<int>(y / DETECTOR_SIZE * rows);

  // Clamp to grid bounds
  cell_x = std::max(0, std::min(static_cast<int>(cols - 1), cell_x));
  cell_y = std::max(0, std::min(static_cast<int>(rows - 1), cell_y));

  return cell_y * cols + cell_x;
}

void GridHitClustering::processGridCell(
    size_t cell_idx, std::vector<TDCHit>::const_iterator begin,
    GridClusteringState& state, std::vector<int>& cluster_labels,
    int& next_cluster_id) const {
  const auto& cell = state.getCell(cell_idx);
  if (cell.hit_indices.empty()) return;

  // Process hits within the cell using a simple connected components approach
  for (size_t i : cell.hit_indices) {
    if (cluster_labels[i] >= 0) continue;  // Already clustered

    // Start new cluster
    int cluster_id = next_cluster_id++;
    std::queue<size_t> to_process;
    to_process.push(i);
    cluster_labels[i] = cluster_id;

    // BFS within the cell
    while (!to_process.empty()) {
      size_t current = to_process.front();
      to_process.pop();
      const auto& current_hit = *(begin + current);

      // Check all other hits in the same cell
      for (size_t j : cell.hit_indices) {
        if (j == current) continue;            // Skip self
        if (cluster_labels[j] >= 0) continue;  // Already clustered

        const auto& other_hit = *(begin + j);
        if (areHitsConnected(current_hit, other_hit)) {
          cluster_labels[j] = cluster_id;
          to_process.push(j);
        }
      }
    }
  }
}

bool GridHitClustering::areHitsConnected(const TDCHit& hit1,
                                         const TDCHit& hit2) const {
  // Check spatial proximity
  double dx = hit1.x - hit2.x;
  double dy = hit1.y - hit2.y;
  double dist_sq = dx * dx + dy * dy;
  double max_dist = config_.grid.connection_distance;

  if (dist_sq > max_dist * max_dist) {
    return false;
  }

  // Check temporal proximity
  uint32_t tof1 = hit1.tof;
  uint32_t tof2 = hit2.tof;
  uint32_t diff = (tof1 > tof2) ? (tof1 - tof2) : (tof2 - tof1);

  // Convert correlation window from ns to 25ns units
  uint32_t window_tof =
      static_cast<uint32_t>(config_.grid.neutron_correlation_window / 25.0);

  return diff <= window_tof;
}

void GridHitClustering::mergeClusterLabels(std::vector<int>& cluster_labels,
                                           int old_label, int new_label) const {
  for (int& label : cluster_labels) {
    if (label == old_label) {
      label = new_label;
    }
  }
}

ClusteringStatistics GridHitClustering::getStatistics(
    const IClusteringState& state, size_t num_hits) const {
  const auto& grid_state = static_cast<const GridClusteringState&>(state);

  ClusteringStatistics stats;
  stats.total_clusters = grid_state.clusters_found;
  stats.unclustered_hits = 0;  // Grid clustering assigns all hits
  for (size_t i = 0; i < num_hits; ++i) {
    if (i < num_hits &&
        *(state.getAlgorithmName().data()) == 'g') {  // Placeholder check
      // Would need cluster_labels to properly count unclustered
    }
  }
  stats.mean_cluster_size =
      (grid_state.clusters_found > 0)
          ? static_cast<double>(num_hits) / grid_state.clusters_found
          : 0.0;
  stats.max_cluster_size = grid_state.max_hits_per_cell;  // Approximation
  stats.total_hits_processed = num_hits;

  return stats;
}

// GridClusteringState Implementation

GridClusteringState::GridClusteringState() { reset(); }

void GridClusteringState::reset() {
  for (auto& cell : grid_cells) {
    cell.clear();
  }

  hits_processed = 0;
  clusters_found = 0;
  cells_with_hits = 0;
  max_hits_per_cell = 0;
}

void GridClusteringState::initializeGrid(size_t cols, size_t rows) {
  grid_cols = cols;
  grid_rows = rows;
  size_t total_cells = cols * rows;

  grid_cells.clear();
  grid_cells.resize(total_cells);
}

void GridClusteringState::insertHit(size_t hit_idx, size_t cell_idx) {
  if (cell_idx < grid_cells.size()) {
    grid_cells[cell_idx].hit_indices.push_back(hit_idx);
  }
}

const GridClusteringState::GridCell& GridClusteringState::getCell(
    size_t cell_idx) const {
  static const GridCell empty_cell;
  return (cell_idx < grid_cells.size()) ? grid_cells[cell_idx] : empty_cell;
}

}  // namespace tdcsophiread