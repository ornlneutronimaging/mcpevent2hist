/**
 * @file benchmakr_raw2hit.cpp
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief benchmark converting raw data to hit performance in single thread
 * @version 0.1
 * @date 2023-08-30
 *
 * @copyright Copyright (c) 2023
 */

#include <chrono>
#include <fstream>
#include <iostream>

#include "tpx3.h"

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

  // first, measure how many hits in the data
  auto hits = parseRawBytesToHits(raw_data);
  std::cout << "Number of hits: " << hits.size() << std::endl;

  // second, measure how long and calculate speed
  const int num_trials = 100;
  double total_time = 0;
  double speed = 0;
  for (int i = 0; i < num_trials; ++i) {
    start = std::chrono::high_resolution_clock::now();
    hits = parseRawBytesToHits(raw_data);
    end = std::chrono::high_resolution_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    speed = hits.size() / (elapsed / 1e6);
    std::cout << "Trial " << i << ":\n"
              << "\t" << elapsed / 1e6 << " s, " << speed << " hits/s" << std::endl;
    total_time += elapsed;
  }
  std::cout << "Average time: " << total_time / num_trials / 1e6 << " s" << std::endl;
  std::cout << "Average speed: " << hits.size() / (total_time / num_trials / 1e6) << " hits/s" << std::endl;
}