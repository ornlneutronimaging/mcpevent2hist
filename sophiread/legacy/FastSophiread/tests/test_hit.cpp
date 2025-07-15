/**
 * @file test_tpx3_fast.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief unit test for Hit class
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

#include "spdlog/spdlog.h"
#include "tpx3_fast.h"

class HitTest : public ::testing::Test {
 protected:
 protected:
  char packet[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  unsigned long long tdc = 8411155;
  unsigned long long gdc = 2000;
  int chip_layout_type = 0;
  Hit hit;

  HitTest() : hit(packet, tdc, gdc, chip_layout_type) {}
};

TEST_F(HitTest, CheckSpidertime) {
  unsigned long long expectedSpidertime = 8411156;  // Expected spidertime
  ASSERT_EQ(expectedSpidertime, hit.getSPIDERTIME());
}

TEST_F(HitTest, CheckSpidertimens) {
  const double expectedSpidertimens = 210278900;  // Expected spidertimens
  ASSERT_DOUBLE_EQ(expectedSpidertimens, hit.getSPIDERTIME_ns());
}

TEST_F(HitTest, CheckTOF) {
  unsigned long long expectedTOF = 1;  // Expected TOF
  ASSERT_EQ(expectedTOF, hit.getTOF());
}

TEST_F(HitTest, CheckTOFns) {
  double expectedTOFns = 25;  // Expected TOFns
  ASSERT_DOUBLE_EQ(expectedTOFns, hit.getTOF_ns());
}

TEST_F(HitTest, CheckXCoordinate) {
  int expectedX = 388;  // Expected X coordinate
  ASSERT_EQ(expectedX, hit.getX());
}

TEST_F(HitTest, CheckYCoordinate) {
  int expectedY = 56;  // Expected Y coordinate
  ASSERT_EQ(expectedY, hit.getY());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}