/**
 * @file main_gdc_extractor.cpp
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief Main entry point for GDC extraction tool
 * @version 0.1
 * @date 2025-02-19
 *
 * @copyright Copyright (c) 2025
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#include <spdlog/spdlog.h>
#include <unistd.h>

#include "gdc_extractor.h"

void print_usage(const char* program_name) {
  spdlog::info(
      "Usage: {} -i <input_tpx3> -o <output_csv> [-c <chunk_size>] [-d] [-v]",
      program_name);
  spdlog::info("Options:");
  spdlog::info("  -i <input_tpx3>    Input TPX3 file");
  spdlog::info("  -o <output_csv>    Output CSV file");
  spdlog::info("  -c <chunk_size>    Chunk size in MB (default: 5120)");
  spdlog::info("  -d                 Enable debug logging");
  spdlog::info("  -v                 Enable verbose logging");
}

sophiread::GDCExtractorOptions parse_arguments(int argc, char* argv[]) {
  sophiread::GDCExtractorOptions options;
  int opt;

  while ((opt = getopt(argc, argv, "i:o:c:dv")) != -1) {
    switch (opt) {
      case 'i':
        options.input_tpx3 = optarg;
        break;
      case 'o':
        options.output_csv = optarg;
        break;
      case 'c':
        options.chunk_size =
            static_cast<size_t>(std::stoull(optarg)) * 1024 * 1024;
        break;
      case 'd':
        options.debug_logging = true;
        break;
      case 'v':
        options.verbose = true;
        break;
      default:
        print_usage(argv[0]);
        throw std::runtime_error("Invalid argument");
    }
  }

  if (options.input_tpx3.empty() || options.output_csv.empty()) {
    print_usage(argv[0]);
    throw std::runtime_error("Missing required arguments");
  }

  return options;
}

int main(int argc, char* argv[]) {
  try {
    auto options = parse_arguments(argc, argv);

    // Set logging level
    if (options.debug_logging) {
      spdlog::set_level(spdlog::level::debug);
    } else if (options.verbose) {
      spdlog::set_level(spdlog::level::info);
    } else {
      spdlog::set_level(spdlog::level::warn);
    }

    // Process file
    sophiread::GDCExtractor extractor(options);
    if (!extractor.process()) {
      return 1;
    }

    return 0;
  } catch (const std::exception& e) {
    spdlog::error("Error: {}", e.what());
    return 1;
  }
}