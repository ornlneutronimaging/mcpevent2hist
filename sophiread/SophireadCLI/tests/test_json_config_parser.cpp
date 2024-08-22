/**
 * @file test_json_config_parser.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Unit tests for JSONConfigParser class.
 * @date 2024-08-16
 *
 * @copyright Copyright (c) 2024
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

#include <cmath>
#include <fstream>

#include "json_config_parser.h"

class JSONConfigParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // This setup will be used for the uniform binning test
    std::ofstream config_file("test_config_uniform.json");
    config_file << R"({
            "abs": {
                "radius": 6.0,
                "min_cluster_size": 2,
                "spider_time_range": 80
            },
            "tof_imaging": {
                "uniform_bins": {
                    "num_bins": 1000,
                    "end": 0.0167
                },
                "super_resolution": 2.0
            }
        })";
    config_file.close();

    // Setup for custom binning test
    std::ofstream config_file_custom("test_config_custom.json");
    config_file_custom << R"({
            "abs": {
                "radius": 7.0,
                "min_cluster_size": 3,
                "spider_time_range": 85
            },
            "tof_imaging": {
                "bin_edges": [0, 0.001, 0.002, 0.005, 0.01, 0.0167]
            }
        })";
    config_file_custom.close();

    // Setup for default values test
    std::ofstream config_file_default("test_config_default.json");
    config_file_default << R"({
            "abs": {}
        })";
    config_file_default.close();
  }

  void TearDown() override {
    std::remove("test_config_uniform.json");
    std::remove("test_config_custom.json");
    std::remove("test_config_default.json");
  }
};

TEST_F(JSONConfigParserTest, ParsesSuperResolutionCorrectly) {
  auto config = JSONConfigParser::fromFile("test_config_uniform.json");
  EXPECT_DOUBLE_EQ(config.getSuperResolution(), 2.0);
}

TEST_F(JSONConfigParserTest, ParsesUniformConfigCorrectly) {
  auto config = JSONConfigParser::fromFile("test_config_uniform.json");

  EXPECT_DOUBLE_EQ(config.getABSRadius(), 6.0);
  EXPECT_EQ(config.getABSMinClusterSize(), 2);
  EXPECT_EQ(config.getABSSpiderTimeRange(), 80);

  auto bin_edges = config.getTOFBinEdges();
  EXPECT_EQ(bin_edges.size(), 1001);  // 1000 bins + 1
  EXPECT_DOUBLE_EQ(bin_edges.front(), 0);
  EXPECT_DOUBLE_EQ(bin_edges.back(), 0.0167);
}

TEST_F(JSONConfigParserTest, ParsesCustomConfigCorrectly) {
  auto config = JSONConfigParser::fromFile("test_config_custom.json");

  EXPECT_DOUBLE_EQ(config.getABSRadius(), 7.0);
  EXPECT_EQ(config.getABSMinClusterSize(), 3);
  EXPECT_EQ(config.getABSSpiderTimeRange(), 85);

  auto bin_edges = config.getTOFBinEdges();
  EXPECT_EQ(bin_edges.size(), 6);
  EXPECT_DOUBLE_EQ(bin_edges[0], 0);
  EXPECT_DOUBLE_EQ(bin_edges[1], 0.001);
  EXPECT_DOUBLE_EQ(bin_edges[2], 0.002);
  EXPECT_DOUBLE_EQ(bin_edges[3], 0.005);
  EXPECT_DOUBLE_EQ(bin_edges[4], 0.01);
  EXPECT_DOUBLE_EQ(bin_edges[5], 0.0167);
}

TEST_F(JSONConfigParserTest, UsesDefaultValuesCorrectly) {
  auto config = JSONConfigParser::fromFile("test_config_default.json");

  EXPECT_DOUBLE_EQ(config.getABSRadius(), 5.0);
  EXPECT_EQ(config.getABSMinClusterSize(), 1);
  EXPECT_EQ(config.getABSSpiderTimeRange(), 75);

  auto bin_edges = config.getTOFBinEdges();
  EXPECT_EQ(bin_edges.size(), 1501);  // 1500 bins + 1
  EXPECT_DOUBLE_EQ(bin_edges.front(), 0);
  EXPECT_DOUBLE_EQ(bin_edges.back(), 0.0167);
  EXPECT_DOUBLE_EQ(config.getSuperResolution(), 1.0);
}

TEST_F(JSONConfigParserTest, ThrowsOnMissingFile) {
  EXPECT_THROW(JSONConfigParser::fromFile("non_existent.json"), std::runtime_error);
}

TEST_F(JSONConfigParserTest, ToStringMethodWorksCorrectly) {
  auto config = JSONConfigParser::fromFile("test_config_uniform.json");
  std::string result = config.toString();

  EXPECT_TRUE(result.find("radius=6") != std::string::npos);
  EXPECT_TRUE(result.find("min_cluster_size=2") != std::string::npos);
  EXPECT_TRUE(result.find("spider_time_range=80") != std::string::npos);
  EXPECT_TRUE(result.find("TOF bins=1000") != std::string::npos);
  EXPECT_TRUE(result.find("TOF max=16.7 ms") != std::string::npos);
  EXPECT_TRUE(result.find("Super Resolution=2") != std::string::npos);
}
