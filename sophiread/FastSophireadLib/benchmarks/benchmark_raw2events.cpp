/**
 * @file benchmark_raw2events.cpp
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief benchmark converting raw data to neutron events performance.
 * @version 0.1
 * @date 2023-08-31
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
  auto batches = findTPX3H(raw_data);
  ABS abs_alg(5.0, 1, 75);
  for (auto& tpx3 : batches) {
    extractHits(tpx3, raw_data);
    // fit hits into clusters
    abs_alg.fit(tpx3.hits);
    // get neutron events
    abs_alg.get_events(tpx3.hits);
  }
  end = std::chrono::high_resolution_clock::now();
  // -- gather statistics
  int n_hits = 0;
  for (const auto& tpx3 : batches) {
    auto hits = tpx3.hits;
    n_hits += hits.size();
  }
  std::cout << "Number of hits: " << n_hits << std::endl;
  elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  std::cout << "Single thread processing: " << elapsed / 1e6 << " s" << std::endl;
  auto speed = n_hits / (elapsed / 1e6);
  std::cout << "Single thread processing speed: " << speed << " hits/s" << std::endl;

  // multi-thread processing
  // -- run
  std::cout << "\nMulti-thread processing..." << std::endl;
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
  std::cout << "Number of hits: " << n_hits << std::endl;
  elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  std::cout << "Multi-thread processing: " << elapsed / 1e6 << " s" << std::endl;
  speed = n_hits / (elapsed / 1e6);
  std::cout << "Multi-thread processing speed: " << speed << " hits/s" << std::endl;
}