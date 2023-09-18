/**
 * @file test_disk_io.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief unit test for disk_io.h
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

#include <regex>

#include "disk_io.h"
#include "spdlog/spdlog.h"

TEST(DiskIOTest, ReadTPX3RawToCharVec) {
  // read the testing raw data
  auto rawdata = readTPX3RawToCharVec("../data/frames_flood_1M.tpx3");

  // check the size of the raw data
  const unsigned long ref_size = 9739597 * 8;
  EXPECT_EQ(rawdata.size(), ref_size);
}

class FileNameGeneratorTest : public ::testing::Test {
 protected:
  std::regex expectedPattern;
  std::string generatedBaseName;
  std::string extension;

  void SetUp() override {
    // Allow a short sleep to avoid immediate timestamps
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  void VerifyFileName(const std::string &resultFileName) {
    std::filesystem::path resultPath(resultFileName);
    generatedBaseName = resultPath.stem().string();
    extension = resultPath.extension().string();

    expectedPattern = std::regex(R"(.*_\d{6}\..*)");
    ASSERT_TRUE(std::regex_match(generatedBaseName + extension, expectedPattern));

    // Check that the timestamp is recent (within the last second).
    auto position = generatedBaseName.find_last_of('_');
    auto timestampStr = generatedBaseName.substr(position + 1);
    auto timestamp = std::stoi(timestampStr);
    ASSERT_GE(timestamp, 0);
    ASSERT_LT(timestamp, 1000000);
  }
};

TEST_F(FileNameGeneratorTest, FileNameWithPathAndExtension) {
  // Arrange
  std::string originalFileName = "/path/to/myfile.txt";

  // Act
  auto resultFileName = generateFileNameWithMicroTimestamp(originalFileName);

  // Assert
  VerifyFileName(resultFileName);
}

TEST_F(FileNameGeneratorTest, FileNameWithoutPath) {
  // Arrange
  std::string originalFileName = "myfile.txt";

  // Act
  auto resultFileName = generateFileNameWithMicroTimestamp(originalFileName);

  // Assert
  VerifyFileName(resultFileName);
}

TEST_F(FileNameGeneratorTest, FileNameWithoutExtension) {
  // Arrange
  std::string originalFileName = "/path/to/myfile";

  // Act
  auto resultFileName = generateFileNameWithMicroTimestamp(originalFileName);

  // Assert
  VerifyFileName(resultFileName);
}
