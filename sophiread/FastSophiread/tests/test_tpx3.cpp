/**
 * @file test_tpx3.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief unit test for TPX3 struct
 * @version 0.1
 * @date 2023-09-01
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
#include <gtest/gtest.h>

#include "disk_io.h"
#include "spdlog/spdlog.h"
#include "tpx3_fast.h"

class TPX3Test : public ::testing::Test {
 protected:
  std::size_t index = 0;
  const int num_packets = 10;
  int chip_layout_type = 1;
  TPX3 tpx3;

  TPX3Test() : tpx3(index, num_packets, chip_layout_type) {}

  void SetUp() override {
    char packet[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    unsigned long long tdc = 1000;
    unsigned long long gdc = 2000;
    for (auto i = 0; i < num_packets; ++i) {
      tpx3.emplace_back(packet, tdc, gdc);
    }
  }
};

TEST_F(TPX3Test, CheckIndex) { ASSERT_EQ(index, tpx3.index); }

TEST_F(TPX3Test, CheckNumPackets) { ASSERT_EQ(num_packets, tpx3.num_packets); }

TEST_F(TPX3Test, CheckChipLayoutType) { ASSERT_EQ(chip_layout_type, tpx3.chip_layout_type); }

TEST_F(TPX3Test, CheckHitsSize) { ASSERT_EQ(num_packets, static_cast<int>(tpx3.hits.size())); }

TEST(TPX3FuncTest, TestFindTPX3H) {
  // read the testing raw data
  auto rawdata = readTPX3RawToCharVec("../data/suann_socket_background_serval32.tpx3");

  //
  auto batches = findTPX3H(rawdata);

  // check the size of the raw data
  const size_t size_reference = 81399;
  EXPECT_EQ(batches.size(), size_reference);
}

TEST(TPX3FuncTest, TestExtractHits) {
  // read the testing raw data
  auto rawdata = readTPX3RawToCharVec("../data/suann_socket_background_serval32.tpx3");

  // locate all headers
  auto batches = findTPX3H(rawdata);

  // locate gdc and tdc
  unsigned long tdc_timestamp = 0;
  unsigned long long int gdc_timestamp = 0;
  for (auto& tpx3 : batches) {
    extractTGDC(tpx3, rawdata, tdc_timestamp, gdc_timestamp);
  }

  // extract hits
  for (auto& tpx3 : batches) {
    extractHits(tpx3, rawdata);
  }

  //
  int n_hits = 0;
  for (const auto& tpx3 : batches) {
    auto hits = tpx3.hits;
    n_hits += hits.size();
  }
  const int n_hits_reference = 98533;
  EXPECT_EQ(n_hits, n_hits_reference);

  // make sure no tof is above 16.67 ms
  for (const auto& tpx3 : batches) {
    auto hits = tpx3.hits;
    for (const auto& hit : hits) {
      auto tof_ms = hit.getTOF_ns() * 1e-6;
      EXPECT_LT(tof_ms, 16670);
    }
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}