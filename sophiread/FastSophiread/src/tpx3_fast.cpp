/**
 * @file tpx3_fast.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Implementation for fast tpx3 data processing
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
#include "tpx3_fast.h"

#include <numeric>

/**
 * @brief Locate all TPX3H (chip dataset) in the raw data.
 *
 * @param raw_bytes
 * @return std::vector<TPX3H>
 */
std::vector<TPX3> findTPX3H(const std::vector<char> &raw_bytes) {
  std::vector<TPX3> batches;
  batches.reserve(raw_bytes.size() / 64);  // just a guess here, need more work

  // local variables
  int chip_layout_type = 0;
  int data_packet_size = 0;
  int data_packet_num = 0;

  // find all batches
  const auto iter_begin = raw_bytes.cbegin();
  const auto iter_end = raw_bytes.cend();
  for (auto iter = raw_bytes.cbegin(); iter + 8 < iter_end; iter += 8) {
    const char *char_array = &(*iter);

    // locate the data packet header
    if (char_array[0] == 'T' && char_array[1] == 'P' && char_array[2] == 'X') {
      data_packet_size = ((0xff & char_array[7]) << 8) | (0xff & char_array[6]);
      data_packet_num = data_packet_size >> 3;  // every 8 (2^3) bytes is a data packet
      chip_layout_type = static_cast<int>(char_array[4]);
      batches.emplace_back(static_cast<size_t>(std::distance(iter_begin, iter)), data_packet_num, chip_layout_type);
    }
  }

  return batches;
}

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