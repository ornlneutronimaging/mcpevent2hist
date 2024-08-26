/**
 * @file test_abs.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief  unit test for abs.h
 * @version 0.1
 * @date 2023-09-06
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

#include "abs.h"
#include "spdlog/spdlog.h"

class ABSTest : public ::testing::Test {
 protected:
  std::vector<Hit> data;

  void SetUp() override {
    // Config random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> pos(0, 1);
    std::uniform_real_distribution<> tot(0, 100);
    std::uniform_real_distribution<> toa(0, 1000);
    std::uniform_real_distribution<> ftoa(0, 255);
    std::uniform_real_distribution<> tof(0, 2000);
    std::uniform_real_distribution<> spidertime(-1, 1);

    // make 3 clusters of 100 hits each
    const int ref_num_hits = 100;
    data.reserve(ref_num_hits * 3);

    int x, y, stime;
    // cluster 1 around (50, 50)
    for (int i = 0; i < ref_num_hits; i++) {
      x = 50 + pos(gen);
      y = 50 + pos(gen);
      stime = 10 + spidertime(gen);
      data.emplace_back(x, y, tot(gen), toa(gen), ftoa(gen), tof(gen), stime);
    }
    // cluster 2 around (100, 100)
    for (int i = 0; i < ref_num_hits; i++) {
      x = 100 + pos(gen);
      y = 100 + pos(gen);
      stime = 15 + spidertime(gen);
      data.emplace_back(x, y, tot(gen), toa(gen), ftoa(gen), tof(gen), stime);
    }
    // cluster 3 around (150, 150)
    for (int i = 0; i < ref_num_hits; i++) {
      x = 150 + pos(gen);
      y = 150 + pos(gen);
      stime = 20 + spidertime(gen);
      data.emplace_back(x, y, tot(gen), toa(gen), ftoa(gen), tof(gen), stime);
    }
  }

  void TearDown() override {
    // This method will run after each test
  }
};

// Test the constructor and the methods set_method, get_cluster_labels, and
// reset
TEST_F(ABSTest, ConstructorAndMethods) {
  ABS abs(5.0, 1, 75);

  // Check init state
  std::vector<int> labels = abs.get_cluster_labels();
  const unsigned long ref_val = 0;
  EXPECT_EQ(labels.size(), ref_val);  // Should be empty initially

  // Check reset state
  abs.reset();
  labels = abs.get_cluster_labels();
  EXPECT_EQ(labels.size(), ref_val);  // Should be empty after reset
}

TEST_F(ABSTest, FitNeutronEvents) {
  const double absoulte_pos_error = 0.5;

  ABS abs(5.0, 1, 75);
  abs.fit(data);
  abs.set_method("centroid");
  auto events = abs.get_events(data);

  // Check that there are 3 events
  const unsigned long ref_num_neutron = 3;
  EXPECT_EQ(events.size(), ref_num_neutron);

  // Check position
  EXPECT_NEAR(events[0].getX(), 50, absoulte_pos_error);
  EXPECT_NEAR(events[0].getY(), 50, absoulte_pos_error);
  EXPECT_NEAR(events[1].getX(), 100, absoulte_pos_error);
  EXPECT_NEAR(events[1].getY(), 100, absoulte_pos_error);
  EXPECT_NEAR(events[2].getX(), 150, absoulte_pos_error);
  EXPECT_NEAR(events[2].getY(), 150, absoulte_pos_error);

  // log the neutron events
  spdlog::info("Neutron events:");
  for (auto& event : events) {
    spdlog::info(event.toString());
  }
};