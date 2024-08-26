/**
 * @file test_fastgaussian.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief unit test for fast gaussian peak finding
 * @version 0.1
 * @date 2023-09-07
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

#include <random>

#include "fastgaussian.h"
#include "spdlog/spdlog.h"

class FastGaussianTest : public ::testing::Test {
 protected:
  std::vector<Hit> data;
  const double absolution_tolerance = 1;
  const int num_hit = 1000;

  void SetUp() override {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> pos(200, 2);
    std::normal_distribution<> tot(20, 5);
    std::normal_distribution<> toa(1000, 200);
    std::normal_distribution<> ftoa(16, 8);
    std::normal_distribution<> tof(1000, 2);
    std::normal_distribution<> spidertime(0, 10);

    data.reserve(num_hit);
    for (int i = 0; i < num_hit; i++) {
      data.emplace_back(Hit(pos(gen), pos(gen), tot(gen), toa(gen), ftoa(gen),
                            tof(gen), spidertime(gen)));
    }
  }
};

TEST_F(FastGaussianTest, FastGaussianWeighted) {
  auto alg = std::make_unique<FastGaussian>();
  auto event = alg->fit(data);

  // Check the centroid
  EXPECT_NEAR(event.getX(), 200.0, absolution_tolerance);
  EXPECT_NEAR(event.getY(), 200.0, absolution_tolerance);
  EXPECT_NEAR(event.getTOF(), 1000.0, absolution_tolerance);
}
