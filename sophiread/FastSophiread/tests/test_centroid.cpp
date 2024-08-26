/**
 * @file test_centroid.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief unit test for centroid peak finding
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

#include "centroid.h"
#include "spdlog/spdlog.h"

class CentroidTest : public ::testing::Test {
 protected:
  std::vector<Hit> data;
  const double absolution_tolerance = 0.1;

  void SetUp() override {
    data.push_back(Hit(1750, 2038, 2445, 1428, 3989, 3026, 740));
    data.push_back(Hit(3015, 2073, 3212, 718, 2842, 428, 422));
    data.push_back(Hit(772, 3912, 3133, 2664, 236, 3334, 3134));
  }
};

TEST_F(CentroidTest, CentroidWeighted) {
  auto alg = std::make_unique<Centroid>();
  auto event = alg->fit(data);

  // Check the centroid
  EXPECT_NEAR(event.getX(), 1863.66, absolution_tolerance);
  EXPECT_NEAR(event.getY(), 2718.74, absolution_tolerance);
  EXPECT_NEAR(event.getTOF(), 2262.67, absolution_tolerance);
}

TEST_F(CentroidTest, CentroidWeightedWithScale) {
  const double super_resolution_factor = 2.0;
  auto alg = std::make_unique<Centroid>();
  alg->set_super_resolution_factor(super_resolution_factor);
  auto event = alg->fit(data);

  // Check the centroid
  EXPECT_NEAR(event.getX(), 1863.66 * super_resolution_factor,
              absolution_tolerance);
  EXPECT_NEAR(event.getY(), 2718.74 * super_resolution_factor,
              absolution_tolerance);
  EXPECT_NEAR(event.getTOF(), 2262.67, absolution_tolerance);
}

TEST_F(CentroidTest, CentroidUnweighted) {
  auto alg = std::make_unique<Centroid>(false);
  auto event = alg->fit(data);

  // Check the centroid
  EXPECT_NEAR(event.getX(), 1845.67, absolution_tolerance);
  EXPECT_NEAR(event.getY(), 2674.33, absolution_tolerance);
  EXPECT_NEAR(event.getTOF(), 2262.67, absolution_tolerance);
}