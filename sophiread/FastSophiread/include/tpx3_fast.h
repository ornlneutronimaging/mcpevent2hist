/**
 * @file tpx3_fast.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Header for fast tpx3 data processing
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
#pragma once

#include <string>
#include <vector>

#include "hit.h"

/**
 * @brief Special struct to hold the information about chip dataset position in
 *        the raw charater array.
 * @note  Each TPX3 dataset batch comes from a single sub-chip
 */
struct TPX3 {
  std::size_t index;           // index of the dataset batch in the raw charater array
  const int num_packets;       // number of packets in the dataset batch (time packet and data packet)
  const int chip_layout_type;  // data source (sub-chip ID)
  std::vector<Hit> hits;       // hits extracted from the dataset batch
  std::vector<unsigned long> tdcs;           // tdc extracted from the dataset batch
  std::vector<unsigned long long int> gdcs;  // gdc extracted from the dataset batch

  TPX3(std::size_t index, int num_packets, int chip_layout_type)
      : index(index), num_packets(num_packets), chip_layout_type(chip_layout_type) {
    hits.reserve(num_packets);
    tdcs.reserve(num_packets);
    gdcs.reserve(num_packets);
  };

  void emplace_back(const char* packet, const unsigned long long tdc, const unsigned long long gdc) {
    hits.emplace_back(packet, tdc, gdc, chip_layout_type);
  };
};

template <typename ForwardIt>
std::vector<TPX3> findTPX3H(ForwardIt first, ForwardIt last);
std::vector<TPX3> findTPX3H(const std::vector<char>& raw_bytes);
std::vector<TPX3> findTPX3H(char* raw_bytes, std::size_t size);

template <typename ForwardIter>
void extractTGDC(TPX3& tpx3h, ForwardIter bytes_begin, ForwardIter bytes_end, unsigned long& tdc_timestamp,
                 unsigned long long& gdc_timestamp);
void extractTGDC(TPX3& tpx3h, const std::vector<char>& raw_bytes, unsigned long& tdc_timestamp,
                 unsigned long long& gdc_timestamp);
void extractTGDC(TPX3& tpx3h, char* raw_bytes, std::size_t size, unsigned long& tdc_timestamp,
                 unsigned long long& gdc_timestamp);

template <typename ForwardIter>
void extractHits(TPX3& tpx3h, ForwardIter bytes_begin, ForwardIter bytes_end);
void extractHits(TPX3& tpx3h, const std::vector<char>& raw_bytes);
void extractHits(TPX3& tpx3h, char* raw_bytes, std::size_t size);