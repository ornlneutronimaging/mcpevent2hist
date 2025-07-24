// TDCSophiread Neutron Event Data Structure
// Optimized neutron event representation with sub-pixel precision and TDC
// timing

#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "tdc_hit.h"

namespace tdcsophiread {

/**
 * @brief Neutron event with sub-pixel precision and TDC timing
 *
 * Represents a detected neutron event derived from clustering multiple hits.
 * Provides sub-pixel coordinate precision through peak fitting and consolidates
 * timing/amplitude information from constituent hits.
 *
 * Memory layout optimized for cache efficiency (24 bytes total):
 * - 16 bytes: x, y coordinates (double precision for sub-pixel)
 * - 4 bytes: tof (time-of-flight in 25ns units)
 * - 2 bytes: tot (combined time-over-threshold)
 * - 1 byte: n_hits (cluster size)
 * - 1 byte: chip_id (source chip identifier)
 */
struct TDCNeutron {
  double x;          ///< Sub-pixel X coordinate (super-resolution scaled)
  double y;          ///< Sub-pixel Y coordinate (super-resolution scaled)
  uint32_t tof;      ///< Time-of-flight in 25ns units (same as TDCHit)
  uint16_t tot;      ///< Combined time-over-threshold from all hits
  uint16_t n_hits;   ///< Number of hits in cluster (cluster size)
  uint8_t chip_id;   ///< Chip ID where neutron was detected
  uint8_t reserved;  ///< Reserved for future use (maintains alignment)

  /**
   * @brief Default constructor
   */
  TDCNeutron()
      : x(0.0), y(0.0), tof(0), tot(0), n_hits(0), chip_id(0), reserved(0) {}

  /**
   * @brief Constructor with all parameters
   * @param x_coord Sub-pixel X coordinate
   * @param y_coord Sub-pixel Y coordinate
   * @param time_of_flight TOF in 25ns units
   * @param time_over_threshold Combined TOT
   * @param hit_count Number of hits in cluster
   * @param chip Chip ID
   */
  TDCNeutron(double x_coord, double y_coord, uint32_t time_of_flight,
             uint16_t time_over_threshold, uint16_t hit_count, uint8_t chip)
      : x(x_coord),
        y(y_coord),
        tof(time_of_flight),
        tot(time_over_threshold),
        n_hits(hit_count),
        chip_id(chip),
        reserved(0) {}

  /**
   * @brief Get TOF in nanoseconds
   * @return Time-of-flight in nanoseconds
   */
  double getTOFNanoseconds() const { return tof * 25.0; }

  /**
   * @brief Get TOF in milliseconds
   * @return Time-of-flight in milliseconds
   */
  double getTOFMilliseconds() const { return tof * 25.0 / 1e6; }

  /**
   * @brief Check if neutron is from specified chip
   * @param chip Chip ID to check
   * @return True if neutron originated from specified chip
   */
  bool isFromChip(uint8_t chip) const { return chip_id == chip; }

  /**
   * @brief Get cluster size category
   * @return Cluster size category for analysis
   */
  enum class ClusterSize {
    Single = 1,  ///< Single hit neutron
    Small = 2,   ///< 2-4 hits
    Medium = 5,  ///< 5-10 hits
    Large = 11   ///< >10 hits
  };

  ClusterSize getClusterSizeCategory() const {
    if (n_hits == 1) return ClusterSize::Single;
    if (n_hits <= 4) return ClusterSize::Small;
    if (n_hits <= 10) return ClusterSize::Medium;
    return ClusterSize::Large;
  }

  /**
   * @brief Calculate spatial distance to another neutron
   * @param other Other neutron event
   * @return Euclidean distance in coordinate units
   */
  double distanceTo(const TDCNeutron& other) const {
    double dx = x - other.x;
    double dy = y - other.y;
    return sqrt(dx * dx + dy * dy);
  }

  /**
   * @brief Calculate temporal distance to another neutron
   * @param other Other neutron event
   * @return Time difference in 25ns units
   */
  uint32_t timeDifferenceTo(const TDCNeutron& other) const {
    return (tof > other.tof) ? (tof - other.tof) : (other.tof - tof);
  }
};

/**
 * @brief Neutron statistics for analysis
 */
struct NeutronStatistics {
  size_t total_neutrons;            ///< Total number of neutrons
  size_t single_hit_neutrons;       ///< Neutrons from single hits
  size_t multi_hit_neutrons;        ///< Neutrons from multiple hits
  double mean_cluster_size;         ///< Average hits per neutron
  double mean_tot;                  ///< Average time-over-threshold
  uint32_t tof_min, tof_max;        ///< TOF range in 25ns units
  double x_min, x_max;              ///< X coordinate range
  double y_min, y_max;              ///< Y coordinate range
  std::vector<size_t> chip_counts;  ///< Neutron count per chip

  /**
   * @brief Default constructor
   */
  NeutronStatistics()
      : total_neutrons(0),
        single_hit_neutrons(0),
        multi_hit_neutrons(0),
        mean_cluster_size(0.0),
        mean_tot(0.0),
        tof_min(0),
        tof_max(0),
        x_min(0.0),
        x_max(0.0),
        y_min(0.0),
        y_max(0.0),
        chip_counts(4, 0) {}
};

/**
 * @brief Utilities for neutron data processing
 */
class NeutronUtils {
 public:
  /**
   * @brief Calculate statistics for neutron dataset
   * @param neutrons Vector of neutron events
   * @return Comprehensive neutron statistics
   */
  static NeutronStatistics calculateStatistics(
      const std::vector<TDCNeutron>& neutrons);

  /**
   * @brief Filter neutrons by spatial region of interest
   * @param neutrons Input neutron events
   * @param x_min Minimum X coordinate
   * @param x_max Maximum X coordinate
   * @param y_min Minimum Y coordinate
   * @param y_max Maximum Y coordinate
   * @return Filtered neutron events within ROI
   */
  static std::vector<TDCNeutron> filterByROI(
      const std::vector<TDCNeutron>& neutrons, double x_min, double x_max,
      double y_min, double y_max);

  /**
   * @brief Filter neutrons by time-of-flight range
   * @param neutrons Input neutron events
   * @param tof_min Minimum TOF in 25ns units
   * @param tof_max Maximum TOF in 25ns units
   * @return Filtered neutron events within TOF range
   */
  static std::vector<TDCNeutron> filterByTOF(
      const std::vector<TDCNeutron>& neutrons, uint32_t tof_min,
      uint32_t tof_max);

  /**
   * @brief Filter neutrons by cluster size
   * @param neutrons Input neutron events
   * @param min_hits Minimum cluster size
   * @param max_hits Maximum cluster size
   * @return Filtered neutron events within cluster size range
   */
  static std::vector<TDCNeutron> filterByClusterSize(
      const std::vector<TDCNeutron>& neutrons, uint16_t min_hits,
      uint16_t max_hits);

  /**
   * @brief Filter neutrons by chip ID
   * @param neutrons Input neutron events
   * @param chip_id Target chip ID
   * @return Neutron events from specified chip
   */
  static std::vector<TDCNeutron> filterByChip(
      const std::vector<TDCNeutron>& neutrons, uint8_t chip_id);

  /**
   * @brief Create 2D histogram of neutron positions
   * @param neutrons Input neutron events
   * @param bins_x Number of X bins
   * @param bins_y Number of Y bins
   * @param x_range X coordinate range (auto-detected if empty)
   * @param y_range Y coordinate range (auto-detected if empty)
   * @return 2D histogram as vector of vectors
   */
  static std::vector<std::vector<size_t>> create2DHistogram(
      const std::vector<TDCNeutron>& neutrons, size_t bins_x, size_t bins_y,
      const std::pair<double, double>& x_range = {},
      const std::pair<double, double>& y_range = {});

  /**
   * @brief Create TOF spectrum histogram
   * @param neutrons Input neutron events
   * @param bins Number of TOF bins
   * @param tof_range TOF range in 25ns units (auto-detected if empty)
   * @return TOF histogram (bin centers and counts)
   */
  static std::pair<std::vector<double>, std::vector<size_t>> createTOFSpectrum(
      const std::vector<TDCNeutron>& neutrons, size_t bins,
      const std::pair<uint32_t, uint32_t>& tof_range = {});

  /**
   * @brief Create cluster size distribution
   * @param neutrons Input neutron events
   * @return Pair of (cluster_sizes, counts)
   */
  static std::pair<std::vector<uint16_t>, std::vector<size_t>>
  createClusterSizeDistribution(const std::vector<TDCNeutron>& neutrons);

  /**
   * @brief Convert neutrons to hits format for compatibility
   * @param neutrons Input neutron events
   * @return Vector of TDCHit objects with neutron coordinates
   */
  static std::vector<TDCHit> convertToHits(
      const std::vector<TDCNeutron>& neutrons);

  /**
   * @brief Sort neutrons by time-of-flight
   * @param neutrons Neutron events to sort (modified in-place)
   * @param ascending Sort order (default: ascending)
   */
  static void sortByTOF(std::vector<TDCNeutron>& neutrons,
                        bool ascending = true);

  /**
   * @brief Sort neutrons by cluster size
   * @param neutrons Neutron events to sort (modified in-place)
   * @param ascending Sort order (default: descending for largest first)
   */
  static void sortByClusterSize(std::vector<TDCNeutron>& neutrons,
                                bool ascending = false);

  /**
   * @brief Validate neutron data integrity
   * @param neutrons Input neutron events
   * @return True if all neutrons have valid data
   */
  static bool validateData(const std::vector<TDCNeutron>& neutrons);
};

}  // namespace tdcsophiread