/**
 * @file benchmark_hits2events.cpp
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief benchmark performance of processing vector of hits to vector of events
 * @version 0.1
 * @date 2023-09-07
 *
 * @copyright Copyright (c) 2023
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#include <chrono>
#include <random>

#include "abs.h"
#include "spdlog/spdlog.h"

/**
 * @brief Generate fake hits for benchmarking.
 *
 * @return std::vector<Hit>
 */
std::vector<Hit> fake_hits() {
  std::vector<Hit> hits;
  const int num_clusters = 12000000;
  const int num_hits_per_cluster = 10;
  hits.reserve(num_clusters * num_hits_per_cluster);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> pos(0, 2);
  std::uniform_real_distribution<> tot(0, 100);
  std::uniform_real_distribution<> toa(0, 1000);
  std::uniform_real_distribution<> ftoa(0, 255);
  std::uniform_real_distribution<> tof(0, 2000);
  std::uniform_real_distribution<> spidertime(-1, 1);

  // generate
  for (int i = 0; i < num_clusters; i++) {
    // cluster center
    int x = 10 * i + pos(gen);
    int y = 10 * i + pos(gen);
    int stime = 10 * i + spidertime(gen);
    // cluster
    for (int j = 0; j < num_hits_per_cluster; j++) {
      hits.emplace_back(x, y, tot(gen), toa(gen), ftoa(gen), tof(gen), stime);
    }
  }
  return hits;
}

int main() {
  // create fake hits
  auto hits = fake_hits();
  spdlog::info("Number of hits: {}", hits.size());
  const auto target_time = hits.size() / 120000000.0;
  spdlog::info("Processing time target: {} s", target_time);

  // single thread processing
  // -- run
  spdlog::info("***Single thread processing***");
  auto start = std::chrono::high_resolution_clock::now();
  auto alg = std::make_unique<ABS>(5.0, 1, 75);
  alg->fit(hits);
  auto events = alg->get_events(hits);
  auto end = std::chrono::high_resolution_clock::now();
  // -- gather statistics
  int n_events = events.size();
  spdlog::info("Number of events: {}", n_events);
  auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();
  spdlog::info("Single thread processing: {} s", elapsed / 1e6);
  auto speed = hits.size() / (elapsed / 1e6);
  spdlog::info("Single thread processing speed: {:<e} hits/s", speed);

  spdlog::info(
      "Multi-thread performance is evaluated with raw2events along with the "
      "previous step.");
}