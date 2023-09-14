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
#include <chrono>
#include <fstream>
#include <iostream>

#include "abs.h"
#include "tbb/tbb.h"
#include "tpx3_fast.h"

using namespace std;

/**
 * @brief Read Timepix3 raw data from file to memory for subsequent analysis.
 *
 * @param filepath
 * @return std::vector<char>
 */
std::vector<char> readTimepix3RawFile(const std::string& filepath) {
  // Open the file
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);

  // Check if file is open successfully
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filepath << std::endl;
    exit(EXIT_FAILURE);
  }

  // Get the size of the file
  std::streamsize fileSize = file.tellg();
  file.seekg(0, std::ios::beg);
  std::cout << "File size (bytes): " << fileSize << std::endl;

  // Create a vector to store the data
  std::vector<char> fileData(fileSize);

  // Read the data
  file.read(fileData.data(), fileSize);

  // Close the file
  file.close();

  return fileData;
}

int main(int argc, char* argv[]) {
  // sanity check
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input file>" << std::endl;
    return 1;
  }

  // read raw data
  std::string in_tpx3 = argv[1];
  auto start = std::chrono::high_resolution_clock::now();
  auto raw_data = readTimepix3RawFile(in_tpx3);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  std::cout << "Read raw data: " << elapsed / 1e6 << " s" << std::endl;

  // single thread processing
  // -- run
  std::cout << "\nSingle thread processing..." << std::endl;
  start = std::chrono::high_resolution_clock::now();
  // locate all the TPX3H (chip dataset) in the raw data
  auto batches = findTPX3H(raw_data);

  // extract all tdc and gdc timestamps
  unsigned long tdc_timestamp = 0;
  unsigned long long gdc_timestamp = 0;
  for (auto& tpx3 : batches) {
    extractTGDC(tpx3, raw_data.cbegin(), raw_data.cend(), tdc_timestamp, gdc_timestamp);
  }

  // process all batches to get neutron events
  auto abs_alg = std::make_unique<ABS>(5.0, 1, 75);
  int total_events = 0;
  for (auto& tpx3 : batches) {
    extractHits(tpx3, raw_data);

    abs_alg->reset();
    // fit hits into clusters
    abs_alg->fit(tpx3.hits);
    // get neutron events
    auto events = abs_alg->get_events(tpx3.hits);

    total_events += events.size();
  }
  end = std::chrono::high_resolution_clock::now();
  // -- gather statistics
  int n_hits = 0;
  for (const auto& tpx3 : batches) {
    auto hits = tpx3.hits;
    n_hits += hits.size();
  }
  std::cout << "Number of hits: " << n_hits << std::endl;
  std::cout << "Number of events: " << total_events << std::endl;
  elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  std::cout << "Single thread processing: " << elapsed / 1e6 << " s" << std::endl;
  auto speed = n_hits / (elapsed / 1e6);
  std::cout << "Single thread processing speed: " << speed << " hits/s" << std::endl;

  // multi-thread processing
  // -- run
  std::cout << "\nMulti-thread processing..." << std::endl;
  start = std::chrono::high_resolution_clock::now();
  // locate all the TPX3H (chip dataset) in the raw data, single thread
  auto batches_mt = findTPX3H(raw_data);
  // extract all tdc and gdc timestamps, single thread
  tdc_timestamp = 0;
  gdc_timestamp = 0;
  for (auto& tpx3 : batches_mt) {
    extractTGDC(tpx3, raw_data.cbegin(), raw_data.cend(), tdc_timestamp, gdc_timestamp);
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
  end = std::chrono::high_resolution_clock::now();
  // -- gather statistics
  n_hits = 0;
  for (const auto& tpx3 : batches_mt) {
    auto hits = tpx3.hits;
    n_hits += hits.size();
  }
  std::cout << "Number of hits: " << n_hits << std::endl;
  elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  std::cout << "Multi-thread processing: " << elapsed / 1e6 << " s" << std::endl;
  speed = n_hits / (elapsed / 1e6);
  std::cout << "Multi-thread processing speed: " << speed << " hits/s" << std::endl;
}