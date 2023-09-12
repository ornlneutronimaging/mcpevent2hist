/**
 * @file tpx3_fast.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Implementation for fast tpx3 data processing
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
#include "tpx3_fast.h"

#include <numeric>

/**
 * @brief Templated function to locate all TPX3H (chip dataset) in the raw data.
 *
 * @tparam ForwardIter
 * @param begin
 * @param end
 * @return std::vector<TPX3>
 */
template <typename ForwardIter>
std::vector<TPX3> findTPX3H(ForwardIter begin, ForwardIter end) {
  std::vector<TPX3> batches;
  batches.reserve(std::distance(begin, end) / 64);  // just a guess here, need more work

  // local variables
  int chip_layout_type = 0;
  int data_packet_size = 0;
  int data_packet_num = 0;

  // find all batches
  for (auto iter = begin; std::distance(iter, end) >= 8; std::advance(iter, 8)) {
    const char *char_array = &(*iter);

    // locate the data packet header
    if (char_array[0] == 'T' && char_array[1] == 'P' && char_array[2] == 'X') {
      data_packet_size = ((0xff & char_array[7]) << 8) | (0xff & char_array[6]);
      data_packet_num = data_packet_size >> 3;  // every 8 (2^3) bytes is a data packet
      chip_layout_type = static_cast<int>(char_array[4]);
      batches.emplace_back(static_cast<size_t>(std::distance(begin, iter)), data_packet_num, chip_layout_type);
    }
  }

  return batches;
}

/**
 * @brief Locate all TPX3H (chip dataset) in the raw data.
 *
 * @param raw_bytes
 * @return std::vector<TPX3H>
 */
std::vector<TPX3> findTPX3H(const std::vector<char> &raw_bytes) {
  return findTPX3H(raw_bytes.cbegin(), raw_bytes.cend());
}

/**
 * @brief Locate all TPX3H (chip dataset) in the raw data.
 *
 * @param raw_bytes
 * @return std::vector<TPX3>
 */
std::vector<TPX3> findTPX3H(const char *raw_bytes) { return findTPX3H(std::begin(raw_bytes), std::end(raw_bytes)); }

/**
 * @brief Locate all TPX3H (chip dataset) in the raw data.
 *
 * @param raw_bytes
 * @param size
 * @return std::vector<TPX3>
 */
std::vector<TPX3> findTPX3H(char *raw_bytes, std::size_t size) { return findTPX3H(raw_bytes, raw_bytes + size); }

void extractHits(TPX3 &tpx3h, const std::vector<char> &raw_bytes) {
  // -- TDC
  unsigned long *tdclast;
  unsigned long long mytdc = 0;
  unsigned long TDC_MSB16 = 0;
  unsigned long TDC_LSB32 = 0;
  unsigned long TDC_timestamp = 0;
  // -- GDC
  unsigned long *gdclast;
  unsigned long long mygdc = 0;
  unsigned long Timer_LSB32 = 0;
  unsigned long Timer_MSB16 = 0;
  unsigned long long int GDC_timestamp = 0;  // 48-bit

  auto bytes_iter = raw_bytes.cbegin() + tpx3h.index;
  for (auto j = 0; j < tpx3h.num_packets; ++j) {
    // check if there are enough bytes left
    if (bytes_iter + 8 >= raw_bytes.cend()) {
      continue;
    }

    // move the iterator to the next packet
    bytes_iter = std::next(bytes_iter, 8);
    // convert the iterator to a char array for data extraction
    const char *char_array = &(*bytes_iter);
    // extract the data from the data packet
    if (char_array[7] == 0x6F) {  // TDC data packets
      tdclast = (unsigned long *)(&char_array[0]);
      mytdc = (((*tdclast) >> 12) & 0xFFFFFFFF);  // rick: 0x3fffffff, get 32-bit tdc
      TDC_LSB32 = GDC_timestamp & 0xFFFFFFFF;
      TDC_MSB16 = (GDC_timestamp >> 32) & 0xFFFF;
      if (mytdc < TDC_LSB32) {
        TDC_MSB16++;
      }
      TDC_timestamp = (TDC_MSB16 << 32) & 0xFFFF00000000;
      TDC_timestamp = TDC_timestamp | mytdc;
    } else if ((char_array[7] & 0xF0) == 0x40) {  // GDC data packet
      gdclast = (unsigned long *)(&char_array[0]);
      mygdc = (((*gdclast) >> 16) & 0xFFFFFFFFFFF);
      if (((mygdc >> 40) & 0xF) == 0x4) {
        Timer_LSB32 = mygdc & 0xFFFFFFFF;  // 32-bit
      } else if (((mygdc >> 40) & 0xF) == 0x5) {
        Timer_MSB16 = mygdc & 0xFFFF;  // 16-bit
        GDC_timestamp = Timer_MSB16;
        GDC_timestamp = (GDC_timestamp << 32) & 0xFFFF00000000;
        GDC_timestamp = GDC_timestamp | Timer_LSB32;
      }
    } else if ((char_array[7] & 0xF0) == 0xb0) {  // data packet
      // NOTE: we are implicitly calling the Hit constructor directly within the
      //       vector for speed.
      tpx3h.emplace_back(char_array, TDC_timestamp, GDC_timestamp);
    }
  }
}