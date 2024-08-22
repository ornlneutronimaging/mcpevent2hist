/**
 * @file: test_sophiread_core.cpp
 * @author: Chen Zhang (zhangc@orn.gov)
 * @brief: Unit tests for the Sophiread Core module.
 * @date: 2024-08-21
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

#include <filesystem>
#include <fstream>

#include "json_config_parser.h"
#include "sophiread_core.h"

class SophireadCoreTest : public ::testing::Test {
 protected:
  std::vector<char> generateMockTPX3Data(int num_packets = 10) {
    std::vector<char> data;

    for (int i = 0; i < num_packets; ++i) {
      // Header packet
      data.push_back('T');
      data.push_back('P');
      data.push_back('X');
      data.push_back('3');
      data.push_back(0);  // chip_layout_type
      data.push_back(0);  // some random data
      data.push_back(8);  // data_packet_size (low byte)
      data.push_back(0);  // data_packet_size (high byte)

      // Data packet (8 bytes)
      for (int j = 0; j < 8; ++j) {
        data.push_back(0);
      }
    }
    return data;
  }

  std::vector<TPX3> generateMockTPX3Batches(int num_batches = 2, int hits_per_batch = 5) {
    std::vector<TPX3> batches;
    for (int i = 0; i < num_batches; ++i) {
      TPX3 batch(i * 100, hits_per_batch, i % 3);  // index, num_packets, chip_layout_type

      // Add mock hits
      for (int j = 0; j < hits_per_batch; ++j) {
        char mock_packet[8] = {0};  // Mock packet data
        batch.emplace_back(mock_packet, 1000 + j, 2000 + j);
      }

      // Add mock neutrons (derived from hits)
      for (const auto& hit : batch.hits) {
        batch.neutrons.emplace_back(hit.getX(), hit.getY(), hit.getTOF(), hit.getTOT(),
                                    1  // nHits, assume 1 hit per neutron for simplicity
        );
      }

      batches.push_back(std::move(batch));
    }
    return batches;
  }

  void SetUp() override {
    // Create a small test TPX3 file
    auto test_data = generateMockTPX3Data(100);
    std::ofstream test_file("test.tpx3", std::ios::binary);
    test_file.write(test_data.data(), test_data.size());
    test_file.close();
  }

  void TearDown() override {
    // Remove the test file
    std::filesystem::remove("test.tpx3");
  }
};

TEST_F(SophireadCoreTest, TimedReadDataToCharVec) {
  auto data = sophiread::timedReadDataToCharVec("test.tpx3");
  EXPECT_EQ(data.size(), 1600);  // 100 * (8 + 8) bytes
}

TEST_F(SophireadCoreTest, TimedFindTPX3H) {
  auto rawdata = generateMockTPX3Data(100);
  auto batches = sophiread::timedFindTPX3H(rawdata);
  EXPECT_EQ(batches.size(), 100);
}

TEST_F(SophireadCoreTest, TimedLocateTimeStamp) {
  std::vector<char> raw_data(8000, 'T');         // Simulating TPX3 data
  std::vector<TPX3> batches = {TPX3(0, 10, 0)};  // Create a dummy TPX3 batch
  sophiread::timedLocateTimeStamp(batches, raw_data);
  // Add assertions based on expected behavior
}

TEST_F(SophireadCoreTest, TimedProcessing) {
  std::vector<char> raw_data(8000, 'T');         // Simulating TPX3 data
  std::vector<TPX3> batches = {TPX3(0, 10, 0)};  // Create a dummy TPX3 batch
  JSONConfigParser config = JSONConfigParser::createDefault();
  sophiread::timedProcessing(batches, raw_data, config);
  // Add assertions based on expected behavior
}

TEST_F(SophireadCoreTest, TimedSaveHitsToHDF5) {
  std::vector<TPX3> batches = generateMockTPX3Batches(2, 5);  // Create a dummy TPX3 batch
  sophiread::timedSaveHitsToHDF5("test_hits.h5", batches);
  EXPECT_TRUE(std::filesystem::exists("test_hits.h5"));
  std::filesystem::remove("test_hits.h5");
}

TEST_F(SophireadCoreTest, TimedSaveEventsToHDF5) {
  std::vector<TPX3> batches = generateMockTPX3Batches(2, 5);  // Create a dummy TPX3 batch
  sophiread::timedSaveEventsToHDF5("test_events.h5", batches);
  EXPECT_TRUE(std::filesystem::exists("test_events.h5"));
  std::filesystem::remove("test_events.h5");
}

TEST_F(SophireadCoreTest, TimedCreateTOFImages) {
  std::vector<TPX3> batches = generateMockTPX3Batches(2, 5);  // Create a dummy TPX3 batch
  std::vector<double> tof_bin_edges = {0.0, 0.1, 0.2, 0.3};
  auto images = sophiread::timedCreateTOFImages(batches, 1.0, tof_bin_edges, "neutron");
  EXPECT_EQ(images.size(), 3);  // 3 bins
}

TEST_F(SophireadCoreTest, TimedSaveTOFImagingToTIFF) {
  std::vector<std::vector<std::vector<unsigned int>>> tof_images(
      3, std::vector<std::vector<unsigned int>>(10, std::vector<unsigned int>(10, 1)));
  std::vector<double> tof_bin_edges = {0.0, 0.1, 0.2, 0.3};
  sophiread::timedSaveTOFImagingToTIFF("test_tof", tof_images, tof_bin_edges, "test");
  EXPECT_TRUE(std::filesystem::exists("test_tof/test_bin_0001.tiff"));
  EXPECT_TRUE(std::filesystem::exists("test_tof/test_bin_0002.tiff"));
  EXPECT_TRUE(std::filesystem::exists("test_tof/test_bin_0003.tiff"));
  EXPECT_TRUE(std::filesystem::exists("test_tof/test_Spectra.txt"));
  std::filesystem::remove_all("test_tof");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}