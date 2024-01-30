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

#include <iostream>
#include <numeric>

#define MAX_BATCH_LEN	100000 // enough to process suann_socket_background_serval32.tpx3 without rollover

#ifdef MAX_BATCH_LEN
#include <cstdlib>
#include <climits>
// allow MAX_BATCH_LEN to come from the environment
long unsigned int _get_max_batch_len(void) {
    if (const char* env_p = std::getenv("MAX_BATCH_LEN")) {
        auto max_batch_len = std::strtoul(env_p, NULL, 0);
        // no conversion produce 0
        if (max_batch_len != 0 )
            return max_batch_len;
    }
    return MAX_BATCH_LEN;
}
#endif

/**
 * @brief Templated function to locate all TPX3H (chip dataset) in the raw data.
 *
 * @tparam ForwardIter
 * @param[in] begin
 * @param[in] end
 * @return std::vector<TPX3>
 * @note must be run in a single thread for all the data
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
 * @brief Templated function to locate all TPX3H (chip dataset) in the raw data.
 *
 * @tparam ForwardIter
 * @param[in] begin
 * @param[in] end
 * @param[out] consumed
 * @return std::vector<TPX3>
 * @note will limit the batch size to 100000, will update the number of elements consumed
 */
template <typename ForwardIter>
std::vector<TPX3> findTPX3H(ForwardIter begin, ForwardIter end, std::size_t &consumed) {
  std::vector<TPX3> batches;
  auto len = std::distance(begin, end) / 64;
#ifdef MAX_BATCH_LEN
  auto _max_batch_len = _get_max_batch_len();
  if ( (long unsigned int)len > _max_batch_len ) {
    len = _max_batch_len;
  }
#endif  // MAX_BATCH_LEN
  batches.reserve(len);
  consumed = 0;

  // local variables
  int chip_layout_type = 0;
  int data_packet_size = 0;
  int data_packet_num = 0;

  // find all batches
  for (auto iter = begin; std::distance(iter, end) >= 8; std::advance(iter, 8), consumed += 8) {
    const char *char_array = &(*iter);
#ifdef MAX_BATCH_LEN
    if (batches.size() >= _max_batch_len) {
      break;
    }
#endif  // MAX_BATCH_LEN

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
 * @param[in] raw_bytes
 * @return std::vector<TPX3H>
 */
std::vector<TPX3> findTPX3H(const std::vector<char> &raw_bytes) {
  return findTPX3H(raw_bytes.cbegin(), raw_bytes.cend());
}

/**
 * @brief Locate all TPX3H (chip dataset) in the raw data.
 *
 * @param[in] raw_bytes
 * @param[in] size
 * @return std::vector<TPX3>
 */
std::vector<TPX3> findTPX3H(char *raw_bytes, std::size_t size) {
  return findTPX3H(raw_bytes, raw_bytes + size);
}

/**
 * @brief Locate all TPX3H (chip dataset) in the raw data.
 *
 * @param[in] raw_bytes
 * @param[out] consumed
 * @return std::vector<TPX3H>
 */
std::vector<TPX3> findTPX3H(const std::vector<char> &raw_bytes, std::size_t& consumed) {
  return findTPX3H(raw_bytes.cbegin(), raw_bytes.cend(), consumed);
}

/**
 * @brief Locate all TPX3H (chip dataset) in the raw data.
 *
 * @param[in] raw_bytes
 * @param[in] size
 * @param[out] consumed
 * @return std::vector<TPX3>
 */
std::vector<TPX3> findTPX3H(char *raw_bytes, std::size_t size, std::size_t& consumed) {
  return findTPX3H(raw_bytes, raw_bytes + size, consumed);
}

/**
 * @brief record the given timestamp as starting timestamp of the dataset batch, and evolve the timestamp till the end
 * of the dataset batch.
 *
 * @tparam ForwardIter
 * @param[in, out] tpx3h
 * @param[in] bytes_begin
 * @param[in] bytes_end
 * @param[in, out] gdc_timestamp
 * @param[in, out] tdc_timestamp
 * @param[in, out] timer_lsb32
 */
template <typename ForwardIter>
void updateTimestamp(TPX3 &tpx3h, ForwardIter bytes_begin, ForwardIter bytes_end, unsigned long &tdc_timestamp,
                     unsigned long long int &gdc_timestamp, unsigned long &timer_lsb32) {
  // record the starting timestamp
  tpx3h.tdc_timestamp = tdc_timestamp;
  tpx3h.gdc_timestamp = gdc_timestamp;
  tpx3h.timer_lsb32 = timer_lsb32;

  process_tpx3_packets(tpx3h, bytes_begin, bytes_end, tdc_timestamp, gdc_timestamp, timer_lsb32, false);
}

/**
 * @brief record the given timestamp as starting timestamp of the dataset batch, and evolve the timestamp till the end
 * of the dataset batch.
 *
 * @param[in, out] tpx3h
 * @param[in] raw_bytes
 * @param[in, out] tdc_timestamp
 * @param[in, out] gdc_timestamp
 * @param[in, out] timer_lsb32
 */
void updateTimestamp(TPX3 &tpx3h, const std::vector<char> &raw_bytes, unsigned long &tdc_timestamp,
                     unsigned long long int &gdc_timestamp, unsigned long &timer_lsb32) {
  updateTimestamp(tpx3h, raw_bytes.cbegin(), raw_bytes.cend(), tdc_timestamp, gdc_timestamp, timer_lsb32);
}

/**
 * @brief record the given timestamp as starting timestamp of the dataset batch, and evolve the timestamp till the end
 * of the dataset batch.
 *
 * @param[in, out] tpx3h
 * @param[in] raw_bytes
 * @param[in] size
 * @param[in, out] tdc_timestamp
 * @param[in, out] gdc_timestamp
 * @param[in, out] timer_lsb32
 */
void updateTimestamp(TPX3 &tpx3h, char *raw_bytes, std::size_t size, unsigned long &tdc_timestamp,
                     unsigned long long int &gdc_timestamp, unsigned long &timer_lsb32) {
  updateTimestamp(tpx3h, raw_bytes, raw_bytes + size, tdc_timestamp, gdc_timestamp, timer_lsb32);
}

/**
 * @brief Get the Hits object
 *
 * @tparam ForwardIter
 * @param[in, out] tpx3h
 * @param[in] bytes_begin
 * @param[in] bytes_end
 */
template <typename ForwardIter>
void extractHits(TPX3 &tpx3h, ForwardIter bytes_begin, ForwardIter bytes_end) {
  process_tpx3_packets(tpx3h, bytes_begin, bytes_end, tpx3h.tdc_timestamp, tpx3h.gdc_timestamp, tpx3h.timer_lsb32,
                       true);
}

/**
 * @brief Get the Hits object
 *
 * @param[in, out] tpx3h
 * @param[in] raw_bytes
 */
void extractHits(TPX3 &tpx3h, const std::vector<char> &raw_bytes) {
  extractHits(tpx3h, raw_bytes.cbegin(), raw_bytes.cend());
}

/**
 * @brief Get the Hits object
 *
 * @param[in, out] tpx3h
 * @param[in] raw_bytes
 * @param[in] size
 */
void extractHits(TPX3 &tpx3h, char *raw_bytes, std::size_t size) { extractHits(tpx3h, raw_bytes, raw_bytes + size); }

/**
 * @brief Update the tdc_timestamp with given char_array and gdc_timestamp
 *
 * @param[in] char_array: the char array of the data packet
 * @param[in] gdc_timestamp: the reference gdc_timestamp
 * @param[out] tdc_timestamp: the target to update
 */
void update_tdc_timestamp(const char *char_array, const unsigned long long &gdc_timestamp,
                          unsigned long &tdc_timestamp) {
  // local var for updating tdc_timestamp
  unsigned long *tdclast;
  unsigned long long mytdc = 0;
  unsigned long TDC_MSB16 = 0;
  unsigned long TDC_LSB32 = 0;

  // extract the data from the data packet
  tdclast = (unsigned long *)(&char_array[0]);
  mytdc = (((*tdclast) >> 12) & 0xFFFFFFFF);  // rick: 0x3fffffff, get 32-bit tdc
  TDC_LSB32 = gdc_timestamp & 0xFFFFFFFF;
  TDC_MSB16 = (gdc_timestamp >> 32) & 0xFFFF;
  if (mytdc < TDC_LSB32) {
    TDC_MSB16++;
  }
  tdc_timestamp = (TDC_MSB16 << 32) & 0xFFFF00000000;
  tdc_timestamp = tdc_timestamp | mytdc;
}

/**
 * @brief Update the gdc_timestamp and timer_lsb34 with given char_array
 *
 * @param[in] char_array
 * @param[in, out] timer_lsb32
 * @param[out] gdc_timestamp
 */
void update_gdc_timestamp_and_timer_lsb32(const char *char_array, unsigned long &timer_lsb32,
                                          unsigned long long &gdc_timestamp) {
  // local var for updating gdc_timestamp and timer_lsb32
  unsigned long *gdclast;
  unsigned long long mygdc = 0;

  // process given GDC packet
  gdclast = (unsigned long *)(&char_array[0]);
  mygdc = (((*gdclast) >> 16) & 0xFFFFFFFFFFFF);

  switch (((mygdc >> 40) & 0xF)) {
    case 0x4:
      timer_lsb32 = mygdc & 0xFFFFFFFF;  // 32-bit
      break;

    case 0x5:
      // Serval sometimes report 0 GDC during experiment, so we need to check
      // if the GDC is 0, if so, we use the previous GDC
      unsigned long long gdc_tmp = mygdc & 0xFFFF;
      gdc_tmp = (gdc_tmp << 32) & 0xFFFF00000000;
      gdc_tmp = gdc_tmp | timer_lsb32;

      if (gdc_tmp != 0) {
        gdc_timestamp = gdc_tmp;
      }
      break;
  }
}

/**
 * @brief Main function to process packets (time&data) in the given TPX3 batch
 *
 * @tparam ForwardIter
 * @param[in, out] tpx3h
 * @param[in] bytes_begin
 * @param[in] bytes_end
 * @param[in, out] tdc_timestamp
 * @param[in, out] gdc_timestamp
 * @param[in, out] timer_lsb32
 * @param[in] extract_hits: whether to extract hits from the data packets, or just scan&update the timestamp
 */
template <typename ForwardIter>
void process_tpx3_packets(TPX3 &tpx3h, ForwardIter bytes_begin, ForwardIter bytes_end, unsigned long &tdc_timestamp,
                          unsigned long long int &gdc_timestamp, unsigned long &timer_lsb32, bool extract_hits) {
  // Move to the first packet
  auto bytes_iter = bytes_begin;
  std::advance(bytes_iter, tpx3h.index);

  // Loop over all packets
  for (auto j = 0; j < tpx3h.num_packets; ++j) {
    if (std::next(bytes_iter, 8) >= bytes_end) {
      continue;
    }

    bytes_iter = std::next(bytes_iter, 8);
    const char *char_array = &(*bytes_iter);

    // extract the data from the data packet
    if (char_array[7] == 0x6F) {
      // TDC data packets
      update_tdc_timestamp(char_array, gdc_timestamp, tdc_timestamp);
    } else if ((char_array[7] & 0xF0) == 0x40) {
      // GDC data packet
      update_gdc_timestamp_and_timer_lsb32(char_array, timer_lsb32, gdc_timestamp);
    } else if ((char_array[7] & 0xF0) == 0xb0) {
      if (extract_hits) {
        // Data packet
        if (tdc_timestamp != 0 && gdc_timestamp != 0) {
          tpx3h.emplace_back(char_array, tdc_timestamp, gdc_timestamp);
        }
      }
    }
  }
}
