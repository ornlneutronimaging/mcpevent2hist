/**
 * @file benchmakr_raw2hit.cpp
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief benchmark converting raw data to hit performance.
 * @version 0.1
 * @date 2023-08-30
 *
 * @copyright Copyright (c) 2023
 * BSD 3-Clause License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of ORNL nor the names of its contributors may be used
 * to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <chrono>
#include <fstream>
#include <iostream>

#include "disk_io.h"
#include "spdlog/spdlog.h"
#include "tbb/tbb.h"
#include "tpx3_fast.h"

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
  // -- run
  spdlog::info("***Single thread processing***");
  start = std::chrono::high_resolution_clock::now();
  auto batches = findTPX3H(raw_data);
  for (auto& tpx3 : batches) {
    extractHits(tpx3, raw_data);
  }
  end = std::chrono::high_resolution_clock::now();
  // -- gather statistics
  int n_hits = 0;
  for (const auto& tpx3 : batches) {
    auto hits = tpx3.hits;
    n_hits += hits.size();
  }
  spdlog::info("Number of hits: {}", n_hits);
  elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Single thread processing: {} s", elapsed / 1e6);
  auto speed = n_hits / (elapsed / 1e6);
  spdlog::info("Single thread processing speed: {} hits/s", speed);

  // multi-thread processing
  // -- run
  spdlog::info("***Multi-thread processing***");
  start = std::chrono::high_resolution_clock::now();
  auto batches_mt = findTPX3H(raw_data);
  // use tbb parallel_for to process batches
  tbb::parallel_for(tbb::blocked_range<size_t>(0, batches_mt.size()), [&](const tbb::blocked_range<size_t>& r) {
    for (size_t i = r.begin(); i != r.end(); ++i) {
      auto& tpx3 = batches_mt[i];
      extractHits(tpx3, raw_data);
    }
  });
  end = std::chrono::high_resolution_clock::now();
  // -- gather statistics
  n_hits = 0;
  for (const auto& tpx3 : batches_mt) {
    auto hits = tpx3.hits;
    n_hits += hits.size();
  }
  spdlog::info("Number of hits: {}", n_hits);
  elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Multi-thread processing: {} s", elapsed / 1e6);
  speed = n_hits / (elapsed / 1e6);
  spdlog::info("Multi-thread processing speed: {} hits/s", speed);
}