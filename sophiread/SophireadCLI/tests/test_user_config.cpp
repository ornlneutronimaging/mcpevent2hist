/**
 * @file test_user_config.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Unit tests for UserConfig class.
 * @version 0.1
 * @date 2023-09-18
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
#include <gtest/gtest.h>

#include <fstream>

#include "tof_binning.h"
#include "user_config.h"

// Test default constructor
TEST(UserConfigTest, DefaultConstructor) {
  UserConfig config;
  EXPECT_DOUBLE_EQ(config.getABSRadius(), 5.0);
  EXPECT_EQ(config.getABSMinClusterSize(), 1);
  EXPECT_EQ(config.getABSSpiderTimeRange(), 75);

  auto tof_edges = config.getTOFBinEdges();
  EXPECT_EQ(tof_edges.size(), 1501);  // 1500 bins + 1
  EXPECT_DOUBLE_EQ(tof_edges.front(), 0.0);
  EXPECT_DOUBLE_EQ(tof_edges.back(), 1.0 / 60);
}

// Test parameterized constructor
TEST(UserConfigTest, ParameterizedConstructor) {
  UserConfig config(10.0, 5, 100);
  EXPECT_DOUBLE_EQ(config.getABSRadius(), 10.0);
  EXPECT_EQ(config.getABSMinClusterSize(), 5);
  EXPECT_EQ(config.getABSSpiderTimeRange(), 100);

  // TOF binning should still be default
  auto tof_edges = config.getTOFBinEdges();
  EXPECT_EQ(tof_edges.size(), 1501);
  EXPECT_DOUBLE_EQ(tof_edges.front(), 0.0);
  EXPECT_DOUBLE_EQ(tof_edges.back(), 1.0 / 60);
}

// Test setters
TEST(UserConfigTest, Setters) {
  UserConfig config;
  config.setABSRadius(15.0);
  config.setABSMinClusterSize(10);
  config.setABSSpiderTimeRange(150);

  EXPECT_DOUBLE_EQ(config.getABSRadius(), 15.0);
  EXPECT_EQ(config.getABSMinClusterSize(), 10);
  EXPECT_EQ(config.getABSSpiderTimeRange(), 150);
}

// Test TOF binning setter
TEST(UserConfigTest, TOFBinningSetter) {
  UserConfig config;
  TOFBinning custom_binning;
  custom_binning.num_bins = 1000;
  custom_binning.tof_max = 20000.0;
  config.setTOFBinning(custom_binning);

  auto tof_edges = config.getTOFBinEdges();
  EXPECT_EQ(tof_edges.size(), 1001);  // 1000 bins + 1
  EXPECT_DOUBLE_EQ(tof_edges.front(), 0.0);
  EXPECT_DOUBLE_EQ(tof_edges.back(), 20000.0);
}

// Test custom TOF bin edges
TEST(UserConfigTest, CustomTOFBinEdges) {
  UserConfig config;
  std::vector<double> custom_edges = {0.0, 100.0, 200.0, 300.0, 400.0};
  config.setCustomTOFBinEdges(custom_edges);

  auto tof_edges = config.getTOFBinEdges();
  EXPECT_EQ(tof_edges.size(), 5);
  EXPECT_DOUBLE_EQ(tof_edges[0], 0.0);
  EXPECT_DOUBLE_EQ(tof_edges[1], 100.0);
  EXPECT_DOUBLE_EQ(tof_edges[2], 200.0);
  EXPECT_DOUBLE_EQ(tof_edges[3], 300.0);
  EXPECT_DOUBLE_EQ(tof_edges[4], 400.0);
}

// Test toString method
TEST(UserConfigTest, ToStringMethod) {
  UserConfig config(20.0, 30, 500);
  std::string result = config.toString();
  // print the result
  std::cout << result << std::endl;
  EXPECT_TRUE(result.find("radius=20") != std::string::npos);
  EXPECT_TRUE(result.find("min_cluster_size=30") != std::string::npos);
  EXPECT_TRUE(result.find("spider_time_range=500") != std::string::npos);
  EXPECT_TRUE(result.find("TOF bins=1500") != std::string::npos);
  EXPECT_TRUE(result.find("TOF max=16.6667 ms") != std::string::npos);
}

// Test parsing a valid configuration file
TEST(UserConfigTest, ParseValidConfigurationFile) {
  // Prepare a test config file
  std::ofstream testFile("testConfig.txt");
  testFile << "# ABS\n";
  testFile << "abs_radius 20.0\n";
  testFile << "abs_min_cluster_size 30\n";
  testFile << "spider_time_range 500\n";
  testFile.close();

  UserConfig config = parseUserDefinedConfigurationFile("testConfig.txt");
  EXPECT_DOUBLE_EQ(config.getABSRadius(), 20.0);
  EXPECT_EQ(config.getABSMinClusterSize(), 30);
  EXPECT_EQ(config.getABSSpiderTimeRange(), 500);

  // TOF binning should still be default
  auto tof_edges = config.getTOFBinEdges();
  EXPECT_EQ(tof_edges.size(), 1501);
  EXPECT_DOUBLE_EQ(tof_edges.front(), 0.0);
  EXPECT_DOUBLE_EQ(tof_edges.back(), 1.0 / 60);

  // Cleanup
  std::remove("testConfig.txt");
}

// Test parsing a configuration file with unknown parameters
TEST(UserConfigTest, ParseInvalidConfigurationFile) {
  // Prepare a test config file
  std::ofstream testFile("testInvalidConfig.txt");
  testFile << "# ABS\n";
  testFile << "unknown_parameter 123.45\n";
  testFile.close();

  // It should ignore the unknown parameter and use the default value instead
  UserConfig config = parseUserDefinedConfigurationFile("testInvalidConfig.txt");
  EXPECT_DOUBLE_EQ(config.getABSRadius(), 5.0);   // Default value
  EXPECT_EQ(config.getABSMinClusterSize(), 1);    // Default value
  EXPECT_EQ(config.getABSSpiderTimeRange(), 75);  // Default value

  // Cleanup
  std::remove("testInvalidConfig.txt");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}