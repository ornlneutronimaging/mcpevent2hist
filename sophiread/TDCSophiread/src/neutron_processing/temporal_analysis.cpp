// TDCSophiread Temporal Analysis Implementation
// Statistical hit distribution analysis for temporal batching optimization

#include <algorithm>
#include <cmath>
#include <vector>

#include "neutron_processing/hit_clustering.h"

namespace tdcsophiread {
namespace TemporalBatching {

BatchStatistics analyzeHitDistribution(
    std::vector<TDCHit>::const_iterator begin,
    std::vector<TDCHit>::const_iterator end, int num_pulses,
    double correlation_window) {
  BatchStatistics stats;

  // Check if range is empty
  size_t num_hits = std::distance(begin, end);
  if (num_hits == 0) {
    return stats;
  }

  // Constants for TOF analysis (25ns units)
  const uint32_t correlation_window_tof =
      static_cast<uint32_t>(correlation_window / 25.0);
  const uint32_t pulse_period_tof =
      static_cast<uint32_t>(16.667 * 1e6 / 25.0);  // 16.667ms in 25ns units

  // Detect pulse boundaries by finding TOF resets
  std::vector<size_t> pulse_boundaries;
  pulse_boundaries.push_back(0);

  uint32_t prev_tof = begin->tof;
  auto it = begin;
  size_t index = 0;

  for (++it, ++index; it != end; ++it, ++index) {
    uint32_t current_tof = it->tof;

    if (current_tof < prev_tof && (prev_tof - current_tof) > 10000) {
      pulse_boundaries.push_back(index);
    }
    prev_tof = current_tof;
  }

  if (pulse_boundaries.size() < static_cast<size_t>(num_pulses)) {
    stats.num_pulses_analyzed = std::max(size_t(1), pulse_boundaries.size());
    stats.total_hits_analyzed = num_hits;
    stats.pulse_period_tof = pulse_period_tof;
    stats.mean_hits_per_window = static_cast<double>(num_hits) /
                                 static_cast<double>(stats.num_pulses_analyzed);
    stats.std_hits_per_window = 0.0;
    stats.optimal_window_tof = correlation_window_tof;
    stats.overlap_size = 0;
    return stats;
  }

  // Analyze hit distribution within correlation windows
  std::vector<double> hits_per_window_samples;

  int available_pulse_segments = static_cast<int>(pulse_boundaries.size());

  int actual_pulses_to_analyze = std::min(num_pulses, available_pulse_segments);
  for (int pulse_idx = 0; pulse_idx < actual_pulses_to_analyze; ++pulse_idx) {
    size_t pulse_start = pulse_boundaries[pulse_idx];
    size_t pulse_end =
        (pulse_idx + 1 < static_cast<int>(pulse_boundaries.size()))
            ? pulse_boundaries[pulse_idx + 1]
            : num_hits;

    auto pulse_begin = begin + pulse_start;
    auto pulse_end_it = begin + pulse_end;

    auto window_it = pulse_begin;
    while (window_it < pulse_end_it - 1) {
      uint32_t window_start_tof = window_it->tof;
      uint32_t window_end_tof = window_start_tof + correlation_window_tof;

      size_t hits_in_window = 0;
      auto count_it = window_it;
      while (count_it < pulse_end_it && count_it->tof <= window_end_tof) {
        hits_in_window++;
        ++count_it;
      }

      if (hits_in_window > 0) {
        hits_per_window_samples.push_back(static_cast<double>(hits_in_window));
      }

      auto prev_window_it = window_it;
      uint32_t target_tof = window_start_tof + correlation_window_tof / 4;
      while (window_it < pulse_end_it && window_it->tof < target_tof) {
        ++window_it;
      }

      if (window_it == prev_window_it && window_it < pulse_end_it) {
        ++window_it;
      }
    }
  }

  if (hits_per_window_samples.empty()) {
    stats.mean_hits_per_window = 1.0;
    stats.std_hits_per_window = 0.0;
  } else {
    double sum = 0.0;
    for (double sample : hits_per_window_samples) {
      sum += sample;
    }
    stats.mean_hits_per_window =
        sum / static_cast<double>(hits_per_window_samples.size());

    double variance_sum = 0.0;
    for (double sample : hits_per_window_samples) {
      double diff = sample - stats.mean_hits_per_window;
      variance_sum += diff * diff;
    }
    stats.std_hits_per_window = std::sqrt(
        variance_sum / static_cast<double>(hits_per_window_samples.size()));
  }
  stats.optimal_window_tof = correlation_window_tof;
  stats.overlap_size =
      static_cast<size_t>(std::ceil(3.0 * stats.std_hits_per_window));
  stats.num_pulses_analyzed = actual_pulses_to_analyze;
  stats.pulse_period_tof = pulse_period_tof;
  stats.total_hits_analyzed = num_hits;

  return stats;
}

}  // namespace TemporalBatching
}  // namespace tdcsophiread