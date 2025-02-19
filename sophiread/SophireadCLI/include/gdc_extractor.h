/**
 * @file gdc_extractor.h
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief Command line tool for extracting GDC information from TPX3 files
 * @version 0.1
 * @date 2025-02-19
 *
 * @copyright Copyright (c) 2025
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#pragma once

#include <string>
#include <vector>

#include "gdc_processor.h"

namespace sophiread {

struct GDCExtractorOptions {
  std::string input_tpx3;
  std::string output_csv;
  size_t chunk_size = 5ULL * 1024 * 1024 * 1024;  // Default 5GB
  bool debug_logging = false;
  bool verbose = false;
};

class GDCExtractor {
 public:
  explicit GDCExtractor(const GDCExtractorOptions& options);

  bool process();

 private:
  GDCExtractorOptions options_;
  GDCProcessor processor_;

  bool writeCSVHeader(std::ofstream& file) const;
  bool writeRecords(std::ofstream& file,
                    const std::vector<GDCRecord>& records) const;
};

}  // namespace sophiread