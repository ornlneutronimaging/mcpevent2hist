/**
 * @file: test_sophiread_core.cpp
 * @author: Chen Zhang (zhangc@orn.gov)
 * @brief: Unit tests for the Sophiread Core module.
 * @date: 2024-08-21
 *
 * @copyright Copyright (c) 2024
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "json_config_parser.h"
#include "sophiread_core.h"
#include "tiff_types.h"

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

  std::vector<TPX3> generateMockTPX3Batches(int num_batches = 2,
                                            int hits_per_batch = 5) {
    std::vector<TPX3> batches;
    for (int i = 0; i < num_batches; ++i) {
      TPX3 batch(i * 100, hits_per_batch,
                 i % 3);  // index, num_packets, chip_layout_type

      // Add mock hits
      for (int j = 0; j < hits_per_batch; ++j) {
        char mock_packet[8] = {0};  // Mock packet data
        batch.emplace_back(mock_packet, 1000 + j, 2000 + j);
      }

      // Add mock neutrons (derived from hits)
      for (const auto& hit : batch.hits) {
        batch.neutrons.emplace_back(
            hit.getX(), hit.getY(), hit.getTOF(), hit.getTOT(),
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
  unsigned long tdc_timestamp = 0;
  unsigned long long gdc_timestamp = 0;
  unsigned long timer_lsb32 = 0;
  sophiread::timedLocateTimeStamp(batches, raw_data, tdc_timestamp,
                                  gdc_timestamp, timer_lsb32);
  // Add assertions based on expected behavior
  // NO value to check as we are using dummy data
}

TEST_F(SophireadCoreTest, TimedProcessing) {
  std::vector<char> raw_data(8000, 'T');         // Simulating TPX3 data
  std::vector<TPX3> batches = {TPX3(0, 10, 0)};  // Create a dummy TPX3 batch
  JSONConfigParser config = JSONConfigParser::createDefault();
  sophiread::timedProcessing(batches, raw_data, config, true);
  // Add assertions based on expected behavior
  // No value to check as we are using dummy data
}

TEST_F(SophireadCoreTest, TimedSaveHitsToHDF5) {
  std::vector<TPX3> batches =
      generateMockTPX3Batches(2, 5);  // Create a dummy TPX3 batch
  sophiread::timedSaveHitsToHDF5("test_hits.h5", batches);
  EXPECT_TRUE(std::filesystem::exists("test_hits.h5"));
  std::filesystem::remove("test_hits.h5");
}

TEST_F(SophireadCoreTest, TimedSaveEventsToHDF5) {
  std::vector<TPX3> batches =
      generateMockTPX3Batches(2, 5);  // Create a dummy TPX3 batch
  sophiread::timedSaveEventsToHDF5("test_events.h5", batches);
  EXPECT_TRUE(std::filesystem::exists("test_events.h5"));
  std::filesystem::remove("test_events.h5");
}

TEST_F(SophireadCoreTest, TimedCreateTOFImages) {
  std::vector<TPX3> batches =
      generateMockTPX3Batches(2, 5);  // Create a dummy TPX3 batch
  std::vector<double> tof_bin_edges = {0.0, 0.1, 0.2, 0.3};
  auto images =
      sophiread::timedCreateTOFImages(batches, 1.0, tof_bin_edges, "neutron");
  EXPECT_EQ(images.size(), 3);       // 3 bins
  EXPECT_EQ(images[0].size(), 517);  // Assuming no super resolution
  EXPECT_EQ(images[0][0].size(), 517);
}

TEST_F(SophireadCoreTest, TimedSaveTOFImagingToTIFF) {
  std::vector<std::vector<std::vector<TIFF32Bit>>> tof_images(
      3,
      std::vector<std::vector<TIFF32Bit>>(10, std::vector<TIFF32Bit>(10, 1)));
  std::vector<double> tof_bin_edges = {0.0, 0.1, 0.2, 0.3};
  sophiread::timedSaveTOFImagingToTIFF("test_tof", tof_images, tof_bin_edges,
                                       "test");
  EXPECT_TRUE(std::filesystem::exists("test_tof/test_bin_0001.tiff"));
  EXPECT_TRUE(std::filesystem::exists("test_tof/test_bin_0002.tiff"));
  EXPECT_TRUE(std::filesystem::exists("test_tof/test_bin_0003.tiff"));
  EXPECT_TRUE(std::filesystem::exists("test_tof/test_Spectra.txt"));

  // Check spectra file contents
  std::ifstream spectra_file("test_tof/test_Spectra.txt");
  std::string line;
  std::vector<std::string> lines;
  while (std::getline(spectra_file, line)) {
    lines.push_back(line);
  }
  // close the file
  spectra_file.close();

  EXPECT_EQ(lines.size(), 4);  // Header + 3 data lines
  EXPECT_EQ(lines[0], "shutter_time,counts");
  EXPECT_EQ(lines[1], "0.1,100");  // 10 * 10 * 1 = 100 counts for each bin
  EXPECT_EQ(lines[2], "0.2,100");
  EXPECT_EQ(lines[3], "0.3,100");

  std::filesystem::remove_all("test_tof");
}

TEST_F(SophireadCoreTest, UpdateTOFImages) {
  std::vector<std::vector<std::vector<unsigned int>>> tof_images(
      3, std::vector<std::vector<unsigned int>>(
             517, std::vector<unsigned int>(517, 0)));
  TPX3 batch =
      generateMockTPX3Batches(1, 5)[0];  // Create a single dummy TPX3 batch
  std::vector<double> tof_bin_edges = {0.0, 0.1, 0.2, 0.3};
  double super_resolution = 1.0;

  sophiread::updateTOFImages(tof_images, batch, super_resolution, tof_bin_edges,
                             "neutron");

  // Check if any updates were made to the images
  bool updated = false;
  for (const auto& image : tof_images) {
    for (const auto& row : image) {
      if (std::any_of(row.begin(), row.end(),
                      [](unsigned int val) { return val > 0; })) {
        updated = true;
        break;
      }
    }
    if (updated) break;
  }
  EXPECT_TRUE(updated);
}

TEST_F(SophireadCoreTest, CalculateSpectralCounts) {
  std::vector<std::vector<std::vector<unsigned int>>> tof_images(
      3, std::vector<std::vector<unsigned int>>(
             5, std::vector<unsigned int>(
                    5, 2)));  // 3 bins, 5x5 images, all values 2

  auto spectral_counts = sophiread::calculateSpectralCounts(tof_images);

  EXPECT_EQ(spectral_counts.size(), 3);  // 3 bins
  for (const auto& count : spectral_counts) {
    EXPECT_EQ(count, 50);  // 5 * 5 * 2 = 50 for each bin
  }
}

TEST_F(SophireadCoreTest, WriteSpectralFile) {
  std::vector<uint64_t> spectral_counts = {10, 20, 30};
  std::vector<double> tof_bin_edges = {0.0, 0.1, 0.2, 0.3};
  std::string filename = "test_spectra.txt";

  sophiread::writeSpectralFile(filename, spectral_counts, tof_bin_edges);

  EXPECT_TRUE(std::filesystem::exists(filename));

  // Read and check file contents
  std::ifstream file(filename);
  std::string line;
  std::vector<std::string> lines;
  while (std::getline(file, line)) {
    lines.push_back(line);
  }

  EXPECT_EQ(lines.size(), 4);  // Header + 3 data lines
  EXPECT_EQ(lines[0], "shutter_time,counts");
  EXPECT_EQ(lines[1], "0.1,10");
  EXPECT_EQ(lines[2], "0.2,20");
  EXPECT_EQ(lines[3], "0.3,30");

  std::filesystem::remove(filename);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}