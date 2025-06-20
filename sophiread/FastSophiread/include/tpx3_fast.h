/**
 * @file tpx3_fast.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Header for fast tpx3 data processing
 * @version 0.1
 * @date 2023-08-31
 *
 * @copyright Copyright (c) 2023
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#pragma once

#include <string>
#include <vector>

#include "hit.h"
#include "neutron.h"

/**
 * @brief Special struct to hold the information about chip dataset position in
 *        the raw character array.
 * @note  Each TPX3 dataset batch comes from a single sub-chip
 */
struct TPX3 {
  std::size_t index;  // index of the dataset batch in the raw character array
  const int num_packets;  // number of packets in the dataset batch (time packet
                          // and data packet)
  const int chip_layout_type;     // data source (sub-chip ID)
  std::vector<Hit> hits;          // hits extracted from the dataset batch
  std::vector<Neutron> neutrons;  // neutrons from clustering hits

  unsigned long tdc_timestamp;  // starting tdc timestamp of the dataset batch
  unsigned long long
      gdc_timestamp;          // starting gdc timestamp of the dataset batch
  unsigned long timer_lsb32;  // starting Timer_LSB32 of the dataset batch

  TPX3(std::size_t index, int num_packets, int chip_layout_type)
      : index(index),
        num_packets(num_packets),
        chip_layout_type(chip_layout_type),
        tdc_timestamp(0),
        gdc_timestamp(0),       // Not using GDC by default
        timer_lsb32(0) {        // We don't need timer_lsb32 when not using GDC
    hits.reserve(num_packets);  // assuming 1 hit per data packet
  };

  void emplace_back(const char* packet, const unsigned long long tdc,
                    const unsigned long long gdc) {
    hits.emplace_back(packet, tdc, gdc, chip_layout_type);
  };

  void emplace_back(const char* packet, const unsigned long long tdc) {
    hits.emplace_back(packet, tdc, chip_layout_type);
  };
};

template <typename ForwardIter>
std::vector<TPX3> findTPX3H(ForwardIter first, ForwardIter last);
std::vector<TPX3> findTPX3H(const std::vector<char>& raw_bytes);
std::vector<TPX3> findTPX3H(char* raw_bytes, std::size_t size);
template <typename ForwardIter>
std::vector<TPX3> findTPX3H(ForwardIter first, ForwardIter last,
                            std::size_t& consumed);
std::vector<TPX3> findTPX3H(const std::vector<char>& raw_bytes,
                            std::size_t& consumed);
std::vector<TPX3> findTPX3H(char* raw_bytes, std::size_t size,
                            std::size_t& consumed);

template <typename ForwardIter>
void updateTimestamp(TPX3& tpx3h, ForwardIter bytes_begin,
                     ForwardIter bytes_end, unsigned long& tdc_timestamp,
                     unsigned long long int& gdc_timestamp,
                     unsigned long& timer_lsb32);
void updateTimestamp(TPX3& tpx3h, const std::vector<char>& raw_bytes,
                     unsigned long& tdc_timestamp,
                     unsigned long long int& gdc_timestamp,
                     unsigned long& timer_lsb32);
void updateTimestamp(TPX3& tpx3h, char* raw_bytes, std::size_t size,
                     unsigned long& tdc_timestamp,
                     unsigned long long int& gdc_timestamp,
                     unsigned long& timer_lsb32);

template <typename ForwardIter>
void updateTimestamp(TPX3& tpx3h, ForwardIter bytes_begin,
                     ForwardIter bytes_end, unsigned long& tdc_timestamp);
void updateTimestamp(TPX3& tpx3h, const std::vector<char>& raw_bytes,
                     unsigned long& tdc_timestamp);
void updateTimestamp(TPX3& tpx3h, char* raw_bytes, std::size_t size,
                     unsigned long& tdc_timestamp);

template <typename ForwardIter>
void extractHits(TPX3& tpx3h, ForwardIter bytes_begin, ForwardIter bytes_end);
void extractHits(TPX3& tpx3h, const std::vector<char>& raw_bytes);
void extractHits(TPX3& tpx3h, char* raw_bytes, std::size_t size);
void extractHitsTDC(TPX3& tpx3h, const std::vector<char>& raw_bytes);

void update_tdc_timestamp(const char* char_array,
                          const unsigned long long& gdc_timestamp,
                          unsigned long& tdc_timestamp);

void update_tdc_timestamp(const char* char_array, unsigned long& tdc_timestamp);

void update_gdc_timestamp_and_timer_lsb32(const char* char_array,
                                          unsigned long& timer_lsb32,
                                          unsigned long long& gdc_timestamp);

template <typename ForwardIter>
void process_tpx3_packets(TPX3& tpx3h, ForwardIter bytes_begin,
                          ForwardIter bytes_end, unsigned long& tdc_timestamp,
                          unsigned long long int& gdc_timestamp,
                          unsigned long& timer_lsb32, bool extract_hits = true);

template <typename ForwardIter>
void process_tpx3_packets(TPX3& tpx3h, ForwardIter bytes_begin,
                          ForwardIter bytes_end, unsigned long& tdc_timestamp,
                          bool extract_hits = true);
