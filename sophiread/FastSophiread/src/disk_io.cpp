/**
 * @file disk_io.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Contains utility functions for disk IO.
 * @version 0.1
 * @date 2023-09-01
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
#include "disk_io.h"

#include "spdlog/spdlog.h"

/**
 * @brief Read Timepix3 raw data from file into memory as a vector of char for subsequent analysis.
 *
 * @param tpx3file
 * @return std::vector<char>
 */
std::vector<char> readTPX3RawToCharVec(const std::string& tpx3file) {
  // Open the file
  std::ifstream file(tpx3file, std::ios::binary | std::ios::ate);

  // Check if file is open successfully
  if (!file.is_open()) {
    spdlog::error("Failed to open file: {}", tpx3file);
    exit(EXIT_FAILURE);
  }

  // Get the size of the file
  std::streampos fileSize = file.tellg();
  file.seekg(0, std::ios::beg);
  spdlog::info("File size: {} bytes", static_cast<size_t>(fileSize));

  // Allocate a vector to store the data
  std::vector<char> vec(fileSize);

  // Read the data from file and store it in the vector
  file.read(vec.data(), fileSize);

  // Close the file
  file.close();

  return vec;
}