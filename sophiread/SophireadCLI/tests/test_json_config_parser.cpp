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
#include "json_config_parser.h"
#include <gtest/gtest.h>
#include <fstream>

class JSONConfigParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::ofstream config_file("test_config.json");
        config_file << R"({
            "abs": {
                "radius": 5.0,
                "min_cluster_size": 1,
                "spider_time_range": 75
            },
            "tof_imaging": {
                "uniform_bins": {
                    "start": 0,
                    "end": 16700,
                    "num_bins": 1500
                }
            }
        })";
        config_file.close();
    }

    void TearDown() override {
        std::remove("test_config.json");
    }
};

TEST_F(JSONConfigParserTest, ParsesConfigCorrectly) {
    auto config = JSONConfigParser::fromFile("test_config.json");
    
    EXPECT_DOUBLE_EQ(config.getABSRadius(), 5.0);
    EXPECT_EQ(config.getABSMinClusterSize(), 1);
    EXPECT_EQ(config.getABSSpiderTimeRange(), 75);

    auto bin_edges = config.getTOFBinEdges();
    EXPECT_EQ(bin_edges.size(), 1501);
    EXPECT_DOUBLE_EQ(bin_edges.front(), 0);
    EXPECT_DOUBLE_EQ(bin_edges.back(), 16700);
}

TEST_F(JSONConfigParserTest, ThrowsOnMissingFile) {
    EXPECT_THROW(JSONConfigParser::fromFile("non_existent.json"), std::runtime_error);
}
