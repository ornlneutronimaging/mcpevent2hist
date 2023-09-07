/**
 * @file tpx3_fast.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Header for fast tpx3 data processing
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

  TPX3(std::size_t index, int num_packets, int chip_layout_type)
      : index(index), num_packets(num_packets), chip_layout_type(chip_layout_type) {
    hits.reserve(num_packets);
  };

  void emplace_back(const char* packet, const unsigned long long tdc, const unsigned long long gdc) {
    hits.emplace_back(packet, tdc, gdc, chip_layout_type);
  };
};

std::vector<TPX3> findTPX3H(const std::vector<char>& raw_bytes);
void extractHits(TPX3& tpx3h, const std::vector<char>& raw_bytes);