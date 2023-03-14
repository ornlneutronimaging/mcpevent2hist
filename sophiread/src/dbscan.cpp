#include "dbscan.h"

#include <iostream>
#include <mlpack.hpp>

DBSCAN::TimeClusterInfo::TimeClusterInfo()
    : m_time_mean(0.),
      m_time_sum(0.),
      m_time_min(DBL_MAX),
      m_time_max(DBL_MIN) {
  m_time_cluster_xy_indexes = std::vector<size_t>();
}

DBSCAN::TimeClusterInfo::TimeClusterInfo(const double time,
                                         const size_t xy_index)
    : m_time_mean(time), m_time_sum(time), m_time_min(time), m_time_max(time) {
  m_time_cluster_xy_indexes = std::vector<size_t>{xy_index};
}

/**
 * @brief Run DBSCAN clustering algorithm on the hits
 *
 * @param hits :: hits
 *
 * @note: the current implementation of DBSCAN does not support using customized
 * peak fitting function to calculate the center of each cluster, aka neutron
 * event xy position. It is using a non-weighted centroid method from
 * mlpack::dbscan::DBSCAN<>.
 */
void DBSCAN::fit(const std::vector<Hit>& hits) {
  // force reset
  reset();

  const size_t max_number_of_hits = hits.size();

  // fill ClusterLabels_ with -1
  clusterLabels_.resize(max_number_of_hits, -1);

  if (max_number_of_hits == 0) return;

  if (m_verbose) {
    std::cout << "Number of hits to process: " << max_number_of_hits
              << std::endl;
    std::cout << "Maximum chunk size (hits): " << m_max_hit_chunk_size
              << std::endl;

    if (max_number_of_hits <= m_max_hit_chunk_size)
      std::cout << "Fitting time clusters (1D DBSCAN) on all hits" << std::endl;
    else
      std::cout << "Fitting time clusters (1D DBSCAN) on chunks of hits..."
                << std::endl;
  }

  size_t chunk_size{0};  // either max_chunk_size or the number of unprocessed
                         // hits, whichever is smaller
  std::vector<TimeClusterInfo> all_time_cluster_infos;
  size_t hit_offset{0};  // for example, if the first chunk had N hits, the
                         // second chunk will have an offset of N, etc.

  while (true) {
    // make a chunk
    chunk_size =
        std::min(m_max_hit_chunk_size, max_number_of_hits - hit_offset);
    std::vector<double> chunk;
    // NOTE: we are using TOA ns here, NOT clock cycle.
    std::transform(
        hits.begin() + hit_offset, hits.begin() + hit_offset + chunk_size,
        std::back_inserter(chunk), [](Hit const& h) { return h.getTOA_ns(); });

    // run 1D time clustering on the chunk
    std::vector<size_t> labels;
    std::vector<double> centroids_1D;
    size_t number_of_clusters{0};
    fit1D(chunk, number_of_clusters, labels, centroids_1D);

    std::vector<TimeClusterInfo> time_cluster_infos;
    time_cluster_infos.resize(number_of_clusters);
    std::vector<TimeClusterInfo>
        non_cluster_infos;  // these unassigned data points may still change
                            // their status later, during merging of chunks

    // set the time mean of each new info from the centroids vector
    for (size_t jj = 0; jj < number_of_clusters; jj++)
      time_cluster_infos[jj].m_time_mean = centroids_1D[jj];

    for (size_t ii = 0; ii < labels.size(); ii++) {
      size_t label = labels[ii];
      if (label == SIZE_MAX || label > number_of_clusters - 1) {
        non_cluster_infos.emplace_back(
            TimeClusterInfo(chunk[ii], ii + hit_offset));
        continue;
      }
      TimeClusterInfo& info = time_cluster_infos[label];
      info.m_time_cluster_xy_indexes.push_back(ii + hit_offset);
      info.m_time_sum += chunk[ii];
      info.m_time_min = std::min(info.m_time_min, chunk[ii]);
      info.m_time_max = std::max(info.m_time_max, chunk[ii]);
    }
    // append non-cluster infos considering them as clusters with a single data
    // point each
    time_cluster_infos.insert(time_cluster_infos.end(),
                              non_cluster_infos.begin(),
                              non_cluster_infos.end());

    // append the new infos to the total infos
    all_time_cluster_infos.insert(all_time_cluster_infos.end(),
                                  time_cluster_infos.begin(),
                                  time_cluster_infos.end());

    hit_offset += chunk_size;
    if (max_number_of_hits - hit_offset == 0) break;  // processed all hits
  }

  if (all_time_cluster_infos.empty()) return;

  // merge all chunk results
  std::vector<TimeClusterInfo> merged_time_cluster_infos;
  mergeTimeClusters1D(all_time_cluster_infos, merged_time_cluster_infos);

  // run 2D clustering of XY points for each time cluster satisfying the
  // min_points size
  assert(m_events.empty());  // must run reset() before fitting

  if (m_verbose) {
    std::cout << "Number of time clusters: " << merged_time_cluster_infos.size()
              << std::endl;
    std::cout << "Fitting XY clusters (2D DBSCAN) on every time cluster..."
              << std::endl;
    std::cout << "Eps: " << m_eps_xy << "; min_points: " << m_min_points_xy
              << std::endl;
  }

  size_t label_offset{0};
  for (auto& info : merged_time_cluster_infos) {
    assert(info.m_time_cluster_xy_indexes.size() >= m_min_points_time);

    std::vector<std::pair<double, double>> xy_points;
    for (auto& index : info.m_time_cluster_xy_indexes)
      xy_points.push_back(
          std::pair<double, double>(hits[index].getX(), hits[index].getY()));

    std::vector<size_t> labels;
    std::vector<std::pair<double, double>> centroids_2D;
    size_t number_of_clusters{0};
    fit2D(xy_points, number_of_clusters, labels, centroids_2D);

    // set cluster labels to all hits contained in the current time cluster
    for (size_t ii = 0; ii < labels.size(); ii++) {
      size_t label = labels[ii];
      int cluster_label = (label == SIZE_MAX || label > number_of_clusters - 1)
                              ? -1
                              : label + label_offset;
      clusterLabels_[info.m_time_cluster_xy_indexes[ii]] = cluster_label;
    }
    label_offset += number_of_clusters;

    std::map<size_t, size_t>
        label_counts;  // label vs. number of xy points with that label
    for (size_t ii = 0; ii < labels.size(); ii++) {
      size_t label = labels[ii];
      if (label == SIZE_MAX || label > number_of_clusters - 1) continue;
      if (label_counts.count(label) == 0) label_counts[label] = 0;
      label_counts[label]++;
    }

    // append events
    // note: currently the tot for each neutron event is set to 0
    // as there is no simple solution to incorporate tot info using DBSCAN
    for (auto const& label_count : label_counts)
      m_events.emplace_back(
          NeutronEvent(centroids_2D[label_count.first].first * DSCALE /*X*/,
                       centroids_2D[label_count.first].second * DSCALE /*Y*/,
                       info.m_time_mean,0, label_count.second));  
  }
}

/**
 * @brief Merge time cluster infos
 *
 * @param input_infos :: unmerged time cluster infos
 * @param merged_infos :: (output) merged time cluster infos
 */
void DBSCAN::mergeTimeClusters1D(std::vector<TimeClusterInfo>& input_infos,
                                 std::vector<TimeClusterInfo>& merged_infos) {
  if (m_verbose) {
    std::cout << "Merging time clusters..." << std::endl;
  }

  // before merging, sort the infos by the min time
  std::sort(input_infos.begin(), input_infos.end(),
            [](const TimeClusterInfo& a, const TimeClusterInfo& b) {
              return a.m_time_min < b.m_time_min;
            });

  merged_infos.clear();
  std::vector<TimeClusterInfo>::const_iterator it = input_infos.begin();
  TimeClusterInfo current = *(it)++;
  TimeClusterInfo next;
  while (it != input_infos.end()) {
    next = *(it);
    if (current.m_time_max > next.m_time_min ||
        next.m_time_min - current.m_time_max <=
            m_eps_time) {  // checking if the clusters should be merged
      current.m_time_max = std::max(current.m_time_max, next.m_time_max);
      current.m_time_min = std::min(current.m_time_min, next.m_time_min);
      current.m_time_cluster_xy_indexes.insert(
          current.m_time_cluster_xy_indexes.end(),
          next.m_time_cluster_xy_indexes.begin(),
          next.m_time_cluster_xy_indexes.end());
      current.m_time_sum += next.m_time_sum;
    } else {
      if (current.m_time_cluster_xy_indexes.size() >= m_min_points_time) {
        current.m_time_mean =
            current.m_time_sum / current.m_time_cluster_xy_indexes.size();
        merged_infos.push_back(current);
      }
      current = *(it);
    }
    it++;
  }
  if (current.m_time_cluster_xy_indexes.size() >= m_min_points_time) {
    current.m_time_mean =
        current.m_time_sum / current.m_time_cluster_xy_indexes.size();
    merged_infos.push_back(current);
  }
}

/**
 * @brief Run DBSCAN algorithm in 1D
 *
 * @param data :: input data
 * @param number_of_clusters :: (output) number of found clusters
 * @param labels :: (output) cluster labels
 * @param centroids :: (output) cluster centroids
 */
void DBSCAN::fit1D(std::vector<double>& data, size_t& number_of_clusters,
                   std::vector<size_t>& labels,
                   std::vector<double>& centroids) {
  // create an arma matrix from the data vector
  arma::mat data_mat(
      &data[0], 1 /*nrows*/, data.size() /*ncols*/,
      false /*arma::mat will re-use the input data vector memory*/);

  // create the dbscan object
  mlpack::DBSCAN<mlpack::RangeSearch<>, mlpack::OrderedPointSelection> dbs(
      m_eps_time, m_min_points_time);

  // run mlpack clustering
  arma::Row<size_t> labels_row;
  arma::mat centroids_mat;
  number_of_clusters = dbs.Cluster(data_mat, labels_row, centroids_mat);

  if (m_verbose) {
    std::cout << "Eps: " << m_eps_time << "; min_points: " << m_min_points_time
              << "; time clusters: " << number_of_clusters << std::endl;
  }

  // fill in the labels vector from the arma label row
  labels_row.for_each([&labels](size_t& val) { labels.push_back(val); });

  // fill in the centroids vector from the arma centroids matrix
  centroids_mat.for_each(
      [&centroids](arma::mat::elem_type& val) { centroids.push_back(val); });
}

/**
 * @brief Run DBSCAN algorithm in 2D
 *
 * @param data :: input data
 * @param number_of_clusters :: (output) number of found clusters
 * @param labels :: (output) cluster labels
 * @param centroids :: (output) cluster centroids
 */
void DBSCAN::fit2D(std::vector<std::pair<double, double>>& data,
                   size_t& number_of_clusters, std::vector<size_t>& labels,
                   std::vector<std::pair<double, double>>& centroids) {
  // create an arma matrix from the data vector
  arma::mat data_mat(
      &(data[0].first), 2 /*nrows*/, data.size() /*ncols*/,
      false /*arma::mat will re-use the input data vector memory*/);

  // create the dbscan object
  mlpack::DBSCAN<mlpack::RangeSearch<>, mlpack::OrderedPointSelection> dbs(
      m_eps_xy, m_min_points_xy);

  // run mlpack clustering
  arma::Row<size_t> labels_row;
  arma::mat centroids_mat;
  number_of_clusters = dbs.Cluster(data_mat, labels_row, centroids_mat);

  // fill in the labels vector from the arma label row
  labels_row.for_each([&labels](size_t& val) { labels.push_back(val); });

  // fill in the centroids vector from the arma centroids matrix
  centroids_mat.each_col([&centroids](const arma::vec& b) {
    centroids.push_back(std::pair<double, double>(b[0], b[1]));
  });
}

/**
 * @brief Run DBSCAN clustering on the hits and return neutron events
 *
 * @param hits :: input hits
 * @return neutron events
 */
std::vector<NeutronEvent> DBSCAN::get_events(const std::vector<Hit>& hits) {
  if (m_events.size() == 0) {
    fit(hits);
  }
  if (m_verbose)
    std::cout << "Total number of events: " << m_events.size() << std::endl;
  return m_events;
}

/**
 * @brief Reset DBSCAN clustering
 */
void DBSCAN::reset() { m_events.clear(); }

/**
 * @brief Get DBSCAN cluster labels for each hit
 */
std::vector<int> DBSCAN::get_cluster_labels() { return clusterLabels_; }
