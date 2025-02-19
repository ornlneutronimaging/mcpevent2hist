/**
 * @file gdc_extractor.cpp
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief Implementation of GDC extraction functionality
 * @version 0.1
 * @date 2025-02-19
 *
 * @copyright Copyright (c) 2025
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#include "gdc_extractor.h"

#include <spdlog/spdlog.h>

#include <fstream>

#include "disk_io.h"

namespace sophiread {

GDCExtractor::GDCExtractor(const GDCExtractorOptions& options)
    : options_(options) {}

bool GDCExtractor::writeCSVHeader(std::ofstream& file) const {
  try {
    file << "chip_id,gdc_value,file_offset,timestamp_ns\n";
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to write CSV header: {}", e.what());
    return false;
  }
}

bool GDCExtractor::writeRecords(std::ofstream& file,
                                const std::vector<GDCRecord>& records) const {
  try {
    for (const auto& record : records) {
      file << record.chip_id << "," << record.gdc_value << ","
           << record.file_offset << "," << (record.gdc_value * 25)
           << "\n";  // Convert to nanoseconds
    }
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to write records: {}", e.what());
    return false;
  }
}

bool GDCExtractor::process() {
  spdlog::info("Processing file: {}", options_.input_tpx3);

  // Open output CSV file
  std::ofstream csv_file(options_.output_csv);
  if (!csv_file.is_open()) {
    spdlog::error("Failed to open output file: {}", options_.output_csv);
    return false;
  }

  // Write CSV header
  if (!writeCSVHeader(csv_file)) {
    return false;
  }

  // Setup file reader
  TPX3FileReader file_reader(options_.input_tpx3);
  const size_t total_size = file_reader.getTotalSize();
  size_t processed_size = 0;

  // Process chunks
  while (!file_reader.isEOF()) {
    auto chunk = file_reader.readChunk(options_.chunk_size);
    if (chunk.empty()) break;

    // Process chunk
    auto records =
        processor_.processChunk(chunk.data(), chunk.size(), processed_size);

    // Write records
    if (!writeRecords(csv_file, records)) {
      return false;
    }

    // Update progress
    processed_size += chunk.size();
    float progress = static_cast<float>(processed_size) / total_size * 100.0f;
    spdlog::info("Progress: {:.2f}%", progress);
  }

  spdlog::info("GDC extraction completed successfully");
  return true;
}

}  // namespace sophiread