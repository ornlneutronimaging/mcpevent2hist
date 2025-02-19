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

bool GDCExtractorOptions::validate() const {
  // Check input file existence
  if (!std::filesystem::exists(input_tpx3)) {
    spdlog::error("Input file does not exist: {}", input_tpx3);
    return false;
  }

  // Check input file is readable
  std::ifstream test_input(input_tpx3);
  if (!test_input.good()) {
    spdlog::error("Input file is not readable: {}", input_tpx3);
    return false;
  }
  test_input.close();

  // Check output directory exists or can be created
  std::filesystem::path output_path =
      std::filesystem::path(output_csv).parent_path();
  if (!output_path.empty()) {
    std::error_code ec;
    if (!std::filesystem::exists(output_path)) {
      if (!std::filesystem::create_directories(output_path, ec)) {
        spdlog::error("Failed to create output directory: {}",
                      output_path.string());
        return false;
      }
    }
  }

  // Test if output file is writable
  std::ofstream test_output(output_csv, std::ios::app);
  if (!test_output.good()) {
    spdlog::error("Output file is not writable: {}", output_csv);
    return false;
  }
  test_output.close();

  // Validate chunk size
  if (chunk_size < MIN_CHUNK_SIZE || chunk_size > MAX_CHUNK_SIZE) {
    spdlog::error("Invalid chunk size: {}. Must be between {} MB and {} GB",
                  chunk_size / (1024 * 1024), MIN_CHUNK_SIZE / (1024 * 1024),
                  MAX_CHUNK_SIZE / (1024 * 1024 * 1024));
    return false;
  }

  return true;
}

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