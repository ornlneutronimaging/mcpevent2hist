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

#include "user_config.h"

// Test toString method of UserConfig class
TEST(UserConfigTest, ToStringMethod) {
  UserConfig config(20.0, 30, 500000);
  std::string expected = "ABS: radius=20, min_cluster_size=30, spider_time_range=500000";
  ASSERT_EQ(config.toString(), expected);
}

// Test parsing a valid configuration file
TEST(UserConfigTest, ParseValidConfigurationFile) {
  // Prepare a test config file
  std::ofstream testFile("testConfig.txt");
  testFile << "# ABS\n";
  testFile << "abs_radius 20.0\n";
  testFile << "abs_min_cluster_size 30\n";
  testFile << "spider_time_range 500000\n";
  testFile.close();

  UserConfig config = parseUserDefinedConfigurationFile("testConfig.txt");
  ASSERT_DOUBLE_EQ(config.getABSRadius(), 20.0);
  ASSERT_EQ(config.getABSMinClusterSize(), 30);
  ASSERT_EQ(config.getABSSpidertimeRange(), 500000);

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
  ASSERT_DOUBLE_EQ(config.getABSRadius(), 5.0);   // Default value
  ASSERT_EQ(config.getABSMinClusterSize(), 1);    // Default value
  ASSERT_EQ(config.getABSSpidertimeRange(), 75);  // Default value

  // Cleanup
  std::remove("testInvalidConfig.txt");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
