/**
 * @file benchmark_raw2events.cpp
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief benchmark converting raw data to neutron events performance.
 * @version 0.1
 * @date 2023-08-31
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
#include <spdlog/spdlog.h>
#include <tbb/tbb.h>

#include <chrono>
#include <fstream>
#include <iostream>

#include "abs.h"
#include "disk_io.h"
#include "tpx3_fast.h"

/**
 * @brief check if any hit has TOF larger than 16.67 ms
 *
 * @param batches
 */
void check_bad_tof(const std::vector<TPX3> batches) {
  // sanity check: hit.getTOF() should be smaller than 666,667 clock, which is
  //               equivalent to 16.67 ms
  int n_bad_hits = 0;
  int n_hits = 0;
  for (const auto& tpx3 : batches) {
    n_hits += tpx3.hits.size();
    for (const auto& hit : tpx3.hits) {
      auto tof_ms = hit.getTOF_ns() * 1e-6;
      if (tof_ms > 16.67) {
        spdlog::error("TOF: {} ms", tof_ms);
        n_bad_hits++;
      } else {
        spdlog::debug("TOF: {} ms", tof_ms);
      }
    }
  }
  spdlog::info("bad/total hits: {}/{}", n_bad_hits, n_hits);
}

/**
 * @brief benchmark converting raw data to neutron events performance, single thread.
 *
 * @param raw_data
 */
void run_single_thread(std::vector<char> raw_data, bool check_tof = false) {
  spdlog::info("Single thread processing...");
  double total_time = 0;
  // locate all headers
  auto start = std::chrono::high_resolution_clock::now();
  auto batches = findTPX3H(raw_data);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Locate all headers: {} s", elapsed / 1e6);
  total_time += elapsed / 1e6;
  // locate all gdc timestamps
  start = std::chrono::high_resolution_clock::now();
  unsigned long tdc_timestamp = 0;
  unsigned long long gdc_timestamp = 0;
  unsigned long timer_lsb32 = 0;
  for (auto& tpx3 : batches) {
    updateTimestamp(tpx3, raw_data, tdc_timestamp, gdc_timestamp, timer_lsb32);
  }
  end = std::chrono::high_resolution_clock::now();
  elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Locate all gdc timestamps: {} s", elapsed / 1e6);
  total_time += elapsed / 1e6;
  /*
  The output below tells us that
  - some header contains more than 1 gdc timestamp
  - some header contains 0 gdc timestamp, therefore relies on the one
    from previous block
  ...
  [2023-09-15 20:09:00.407] [debug] --------------------
  [2023-09-15 20:09:00.407] [debug] GDC: 4600041423
  [2023-09-15 20:09:00.407] [debug] GDC: 4600020997
  [2023-09-15 20:09:00.407] [debug] --------------------
  [2023-09-15 20:09:00.407] [debug] GDC: 4600020997
  [2023-09-15 20:09:00.407] [debug] GDC: 4600056518
  [2023-09-15 20:09:00.407] [debug] --------------------
  [2023-09-15 20:09:00.407] [debug] GDC: 4600056518
  [2023-09-15 20:09:00.407] [debug] --------------------
  [2023-09-15 20:09:00.407] [debug] GDC: 4600056518
  [2023-09-15 20:09:00.407] [debug] --------------------
  [2023-09-15 20:09:00.407] [debug] GDC: 4600056518
  [2023-09-15 20:09:00.407] [debug] --------------------
  [2023-09-15 20:09:00.407] [debug] GDC: 4600056518
  ...
  */
  // get all hits
  start = std::chrono::high_resolution_clock::now();
  auto abs_alg = std::make_unique<ABS>(5.0, 1, 75);
  for (auto& tpx3 : batches) {
    extractHits(tpx3, raw_data);

    abs_alg->reset();
    // fit hits into clusters
    abs_alg->fit(tpx3.hits);
    // get neutron events
    auto events = abs_alg->get_events(tpx3.hits);
  }
  end = std::chrono::high_resolution_clock::now();
  elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Get all hits: {} s", elapsed / 1e6);
  total_time += elapsed / 1e6;
  // find out total number of hits
  int n_hits = 0;
  for (const auto& tpx3 : batches) {
    auto hits = tpx3.hits;
    n_hits += hits.size();
  }
  spdlog::info("Total time: {} s", total_time);
  spdlog::info("Number of hits: {}", n_hits);
  auto speed = n_hits / total_time;
  spdlog::info("Single thread processing speed: {:<e} hits/s", speed);

  if (check_tof) {
    check_bad_tof(batches);
  }

  // find the highest spidertime_ns from all hits
  double max_spidertime_ns = 0;
  for (const auto& tpx3 : batches) {
    for (const auto& hit : tpx3.hits) {
      const auto spi_time_ns = hit.getSPIDERTIME_ns();
      if (spi_time_ns > max_spidertime_ns) {
        max_spidertime_ns = spi_time_ns;
      }
    }
  }
  spdlog::info("Max spidertime_ns: {} / s", max_spidertime_ns / 1e9);

  // save all hits to a tmp file, load the spiderTime from the file, and find
  // the max spidertime
  std::string tmp_file = "verify_spidertime.h5";
  // consolidate all hits into a single vector
  std::vector<Hit> hits;
  for (const auto& tpx3 : batches) {
    auto tpx3_hits = tpx3.hits;
    hits.insert(hits.end(), tpx3_hits.begin(), tpx3_hits.end());
  }
  // save hits to HDF5 file
  saveHitsToHDF5(tmp_file, hits);
  // load spiderTime_ns from HDF5 file
  H5::H5File file(tmp_file, H5F_ACC_RDONLY);
  H5::Group group = file.openGroup("hits");
  H5::DataSet spidertime_dataset = group.openDataSet("spidertime_ns");
  // read data
  std::vector<double> spidertime_ns;
  spidertime_ns.resize(spidertime_dataset.getSpace().getSimpleExtentNpoints());
  spidertime_dataset.read(spidertime_ns.data(), H5::PredType::NATIVE_DOUBLE);
  // find the max spidertime
  double max_spidertime_ns_from_file = 0;
  for (const auto& spi_time_ns : spidertime_ns) {
    if (spi_time_ns > max_spidertime_ns_from_file) {
      max_spidertime_ns_from_file = spi_time_ns;
    }
  }
  spdlog::info("Max spidertime_ns from file: {} / s", max_spidertime_ns_from_file / 1e9);
}

void run_multi_thread(std::vector<char> raw_data, bool check_tof = false) {
  spdlog::info("Multi-thread processing...");
  auto start = std::chrono::high_resolution_clock::now();
  // locate all the TPX3H (chip dataset) in the raw data, single thread
  auto batches_mt = findTPX3H(raw_data);
  // update starting time stamp for all headers, single thread
  unsigned long tdc_timestamp = 0;
  unsigned long long gdc_timestamp = 0;
  unsigned long timer_lsb32 = 0;
  for (auto& tpx3 : batches_mt) {
    updateTimestamp(tpx3, raw_data, tdc_timestamp, gdc_timestamp, timer_lsb32);
  }
  // use tbb parallel_for to process batches
  tbb::parallel_for(tbb::blocked_range<size_t>(0, batches_mt.size()), [&](const tbb::blocked_range<size_t>& r) {
    auto abs_alg_mt = std::make_unique<ABS>(5.0, 1, 75);
    for (size_t i = r.begin(); i != r.end(); ++i) {
      auto& tpx3 = batches_mt[i];
      extractHits(tpx3, raw_data);

      abs_alg_mt->reset();
      // fit hits into clusters
      abs_alg_mt->fit(tpx3.hits);
      // get neutron events
      auto events = abs_alg_mt->get_events(tpx3.hits);
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

  if (check_tof) {
    check_bad_tof(batches_mt);
  }
}

int main(int argc, char* argv[]) {
  // set up spdlog
  // NOTE: toggle debug level here to see debug messages
  // spdlog::set_level(spdlog::level::debug);

  // sanity check
  if (argc < 2) {
    spdlog::error("Usage: {} <input file>", argv[0]);
    return 1;
  }

  // read raw data
  std::string in_tpx3 = argv[1];
  auto start = std::chrono::high_resolution_clock::now();
  auto raw_data = readTPX3RawToCharVec(in_tpx3);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Read raw data: {} s", elapsed / 1e6);

  // single thread processing
  run_single_thread(raw_data, true);

  // multi-thread processing
  // run_multi_thread(raw_data, true);
}