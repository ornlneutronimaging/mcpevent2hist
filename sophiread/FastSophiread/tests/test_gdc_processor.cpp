/**
 * @file test_gdc_processor.cpp
 * @brief Unit tests for GDCProcessor class
 */
#include <gtest/gtest.h>

#include <iostream>

#include "gdc_processor.h"

TEST(GDCProcessorTest, TwoChipsSequence) {
  std::vector<char> data(48);
  char* ptr = data.data();

  // Define integer values
  const int timer_lsb32 = 1987;  // 0x07C3
  const int gdc_msb16 = 0x1234;  // Correct MSB16 placement
  const unsigned long long gdc_value =
      (static_cast<unsigned long long>(gdc_msb16) << 32) | timer_lsb32;

  // Construct correct packet values based on the detector reference
  uint64_t lsb32_val = (0x4ULL << 60) | (0x4ULL << 56) | (0x00ULL << 48) |
                       (static_cast<uint64_t>(timer_lsb32) << 16) | 0xAAAA;

  uint64_t gdc_val = (0x4ULL << 60) | (0x5ULL << 56) | (0x00ULL << 32) |
                     (static_cast<uint64_t>(gdc_msb16) << 16) | 0xAAAA;

  // Chip 1 header
  memcpy(ptr, "TPX3", 4);
  ptr[4] = 1;
  ptr[6] = 16;
  ptr[7] = 0;
  ptr += 8;

  // Chip 1 LSB32 packet
  memcpy(ptr, &lsb32_val, 8);
  ptr += 8;

  // Chip 1 GDC packet
  memcpy(ptr, &gdc_val, 8);
  ptr += 8;

  // Chip 2 header
  memcpy(ptr, "TPX3", 4);
  ptr[4] = 2;
  ptr[6] = 16;
  ptr[7] = 0;
  ptr += 8;

  // Chip 2 LSB32 packet
  memcpy(ptr, &lsb32_val, 8);
  ptr += 8;

  // Chip 2 GDC packet
  memcpy(ptr, &gdc_val, 8);

  // Debug: Print raw bytes
  std::cout << "Corrected Mock Data Bytes:" << std::endl;
  for (size_t i = 0; i < data.size(); i += 8) {
    std::cout << "Packet at offset " << i << ": ";
    for (size_t j = 0; j < 8; ++j) {
      printf("%02X ", static_cast<unsigned char>(data[i + j]));
    }
    std::cout << std::endl;
  }

  // Process data
  GDCProcessor processor;
  auto records = processor.processChunk(data.data(), data.size(), 0);

  // Verify
  ASSERT_EQ(records.size(), 2);
  EXPECT_EQ(records[0].gdc_value, gdc_value);
  EXPECT_EQ(records[1].gdc_value, gdc_value);
}
