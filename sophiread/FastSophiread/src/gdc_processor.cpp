/**
 * @file gdc_processor.cpp
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief Extract GDC values from TPX3 raw data
 * @version 0.1
 * @date 2025-02-18
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "gdc_processor.h"

#include <iostream>

#include "spdlog/spdlog.h"

std::vector<GDCRecord> GDCProcessor::processChunk(const char* data, size_t size,
                                                  size_t base_offset) {
  std::vector<GDCRecord> records;
  records.reserve(size / PACKET_SIZE);

  int current_chip_id = -1;
  size_t offset = 0;

  std::cout << "Processing chunk of size: " << size << std::endl;

  while (offset + PACKET_SIZE <= size) {
    const char* char_array = data + offset;

    // Debug print each packet
    std::cout << "Packet at offset " << offset << ": ";
    for (int i = 0; i < 8; i++) {
      std::cout << std::hex << (int)(unsigned char)char_array[i] << " ";
    }
    std::cout << std::endl;

    // Check for TPX3 header
    if (char_array[0] == 'T' && char_array[1] == 'P' && char_array[2] == 'X' &&
        char_array[3] == '3') {
      current_chip_id = static_cast<int>(char_array[4]);
      std::cout << "Found TPX3 header for chip_id: " << current_chip_id
                << std::endl;
      offset += PACKET_SIZE;
      // reset timer_lsb32 and gdc_timestamp to avoid using the values from
      // previous chip timer_lsb32 = 0; gdc_timestamp = 0;
      continue;
    }

    // Check for GDC packet
    if ((char_array[7] & 0xF0) == 0x40) {
      std::cout << "Found GDC packet, current_chip_id: " << current_chip_id
                << std::endl;

      // Debug print the unpacked GDC values
      unsigned long* gdclast = (unsigned long*)(&char_array[0]);
      unsigned long long mygdc = (((*gdclast) >> 16) & 0xFFFFFFFFFFFF);
      bool is_gdc = ((mygdc >> 40) & 0xF) == 0x5;

      std::cout << "gdclast: " << std::hex << *gdclast << std::endl;
      std::cout << "mygdc: " << std::hex << mygdc << std::endl;
      std::cout << "type: " << ((mygdc >> 40) & 0xF) << std::endl;

      // Use our persisted state variables
      update_gdc_timestamp_and_timer_lsb32(char_array, timer_lsb32,
                                           gdc_timestamp);

      std::cout << "After update: timer_lsb32=" << timer_lsb32
                << " gdc_timestamp=" << gdc_timestamp << std::endl;

      if (is_gdc && gdc_timestamp != 0 && current_chip_id >= 0) {
        records.emplace_back(current_chip_id, gdc_timestamp,
                             base_offset + offset);
        std::cout << "Added record for chip " << current_chip_id << std::endl;
      }
    }

    offset += PACKET_SIZE;
  }

  std::cout << "Returning " << records.size() << " records" << std::endl;
  return records;
}
