/**
 * @file gdc_processor.h
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief This file is for evaluating the GDC outside of standard data reduction
 * flow.
 * @version 0.1
 * @date 2025-02-18
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <cstdint>
#include <vector>

#include "tpx3_fast.h"  // we will reuse the tested GDC extractor from TPX3Fast

struct GDCRecord {
  int chip_id;
  uint64_t gdc_value;
  size_t file_offset;

  GDCRecord(int id, uint64_t value, size_t offset)
      : chip_id(id), gdc_value(value), file_offset(offset) {}
};

class GDCProcessor {
 public:
  GDCProcessor() = default;
  ~GDCProcessor() = default;

  std::vector<GDCRecord> processChunk(const char* data, size_t size,
                                      size_t base_offset);

 private:
  static constexpr size_t PACKET_SIZE = 8;
  unsigned long timer_lsb32 = 0;
  unsigned long long gdc_timestamp = 0;
};
