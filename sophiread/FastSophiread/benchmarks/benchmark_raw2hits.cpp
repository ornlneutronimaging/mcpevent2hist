/**
 * @file benchmakr_raw2hit.cpp
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief benchmark converting raw data to hit performance.
 * @version 0.1
 * @date 2023-08-30
 *
 * @copyright Copyright (c) 2023
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <chrono>
#include <fstream>
#include <iostream>

#include "disk_io.h"
#include "hit.h"
#include "spdlog/spdlog.h"
#include "tbb/tbb.h"
#include "tpx3_fast.h"

void run_single_thread(const std::vector<char>& raw_data) {
  // -- run
  spdlog::info("***Single thread processing***");
  auto start = std::chrono::high_resolution_clock::now();
  auto batches = findTPX3H(raw_data);
  for (auto& tpx3 : batches) {
    extractHits(tpx3, raw_data);
  }
  auto end = std::chrono::high_resolution_clock::now();
  // -- gather statistics
  int n_hits = 0;
  for (const auto& tpx3 : batches) {
    auto hits = tpx3.hits;
    n_hits += hits.size();
  }
  spdlog::info("Number of hits: {}", n_hits);
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Single thread processing: {} s", elapsed / 1e6);
  auto speed = n_hits / (elapsed / 1e6);
  spdlog::info("Single thread processing speed: {:<e} hits/s", speed);
}

void run_multi_thread(const std::vector<char>& raw_data) {
  // -- run
  spdlog::info("***Multi-thread processing***");
  auto start = std::chrono::high_resolution_clock::now();
  auto batches_mt = findTPX3H(raw_data);
  // use tbb parallel_for to process batches
  tbb::parallel_for(tbb::blocked_range<size_t>(0, batches_mt.size()), [&](const tbb::blocked_range<size_t>& r) {
    for (size_t i = r.begin(); i != r.end(); ++i) {
      auto& tpx3 = batches_mt[i];
      extractHits(tpx3, raw_data);
    }
  });
  auto end = std::chrono::high_resolution_clock::now();
  // -- gather statistics
  auto n_hits = 0;
  for (const auto& tpx3 : batches_mt) {
    auto hits = tpx3.hits;
    n_hits += hits.size();
  }
  spdlog::info("Number of hits: {}", n_hits);
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Multi-thread processing: {} s", elapsed / 1e6);
  auto speed = n_hits / (elapsed / 1e6);
  spdlog::info("Multi-thread processing speed: {:<e} hits/s", speed);
}

int main(int argc, char* argv[]) {
  // sanity check
  if (argc < 2) {
    spdlog::critical("Usage: {} <input file>", argv[0]);
    return 1;
  }

  // read raw data
  std::string in_tpx3 = argv[1];
  auto start = std::chrono::high_resolution_clock::now();
  auto raw_data = readTPX3RawToCharVec(in_tpx3);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  // logging
  spdlog::info("Read raw data: {} s", elapsed / 1e6);

  // single thread processing
  run_single_thread(raw_data);

  // multi-thread processing
  run_multi_thread(raw_data);
}