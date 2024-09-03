/**
 * @file venus_auto_reducer.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Auto reducer demo application for VENUS@SNS
 * @version 0.1
 * @date 2024-08-21
 *
 * @copyright Copyright (c) 2024
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#include <spdlog/spdlog.h>
#include <tbb/tbb.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "json_config_parser.h"
#include "sophiread_core.h"
namespace fs = std::filesystem;

struct ProgramOptions {
  std::string input_dir;
  std::string output_dir;
  std::string config_file;
  std::string tiff_base = "tof_image";
  std::string tof_mode = "neutron";
  int check_interval = 5;
  bool verbose = false;
  bool debug = false;
};

void print_usage(const char* program_name) {
  spdlog::info(
      "Usage: {} -i <input_dir> -o <output_dir> [-u <user_config_json>] [-f "
      "<tiff_file_name_base>] [-m <tof_mode>] [-c <check_interval>] [-v] [-d]",
      program_name);
  spdlog::info("Options:");
  spdlog::info("  -i <input_dir>    Input directory with TPX3 files");
  spdlog::info("  -o <output_dir>   Output directory for TIFF files");
  spdlog::info("  -u <config_file>  User configuration JSON file (optional)");
  spdlog::info(
      "  -f <tiff_base>    Base name for TIFF files (default: tof_image)");
  spdlog::info(
      "  -m <tof_mode>     TOF mode: 'hit' or 'neutron' (default: neutron)");
  spdlog::info("  -c <interval>     Check interval in seconds (default: 5)");
  spdlog::info("  -d                Debug output");
  spdlog::info("  -v                Verbose output");
}

ProgramOptions parse_arguments(int argc, char* argv[]) {
  ProgramOptions options;
  int opt;

  while ((opt = getopt(argc, argv, "i:o:u:f:m:c:dv")) != -1) {
    switch (opt) {
      case 'i':
        options.input_dir = optarg;
        break;
      case 'o':
        options.output_dir = optarg;
        break;
      case 'u':
        options.config_file = optarg;
        break;
      case 'f':
        options.tiff_base = optarg;
        break;
      case 'm':
        options.tof_mode = optarg;
        break;
      case 'c':
        options.check_interval = std::stoi(optarg);
        break;
      case 'd':
        options.debug = true;
        break;
      case 'v':
        options.verbose = true;
        break;
      default:
        print_usage(argv[0]);
        throw std::runtime_error("Invalid argument");
    }
  }

  // Validate required arguments
  if (options.input_dir.empty() || options.output_dir.empty()) {
    print_usage(argv[0]);
    throw std::runtime_error("Missing required arguments");
  }

  // Validate tof_mode
  if (options.tof_mode != "hit" && options.tof_mode != "neutron") {
    throw std::runtime_error("Invalid TOF mode. Use 'hit' or 'neutron'.");
  }

  // Validate check_interval
  if (options.check_interval <= 0) {
    throw std::runtime_error("Check interval must be a positive integer.");
  }

  return options;
}

/**
 * @brief Process all existing tpx3 files
 *
 * @param[in] input_dir
 * @param[in] output_dir
 * @param[in] tiff_base
 * @param[in] tof_mode
 * @param[in] config
 * @param[in, out] processed_files
 */
void process_existing_files(const std::string& input_dir,
                            const std::string& output_dir,
                            const std::string& tiff_base,
                            const std::string& tof_mode, const IConfig& config,
                            std::unordered_set<std::string>& processed_files) {
  spdlog::info("Processing existing files in {}", input_dir);

  unsigned long tdc_timestamp = 0;
  unsigned long long gdc_timestamp = 0;
  unsigned long timer_lsb32 = 0;

  // NOTE: we need to process files sequentially as we are accumulating the
  // counts to the
  //       same set of tiff files in the output folder
  for (const auto& entry : fs::directory_iterator(input_dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".tpx3") {
      std::string filename = entry.path().stem().string();

      if (processed_files.find(filename) != processed_files.end()) {
        spdlog::debug("Skipping already processed file: {}", filename);
        continue;
      }

      spdlog::info("Processing file: {}", entry.path().string());

      try {
        // Read the TPX3 file
        auto raw_data =
            sophiread::timedReadDataToCharVec(entry.path().string());

        // Find TPX3 headers
        auto batches = sophiread::timedFindTPX3H(raw_data);

        // Process the data
        sophiread::timedLocateTimeStamp(batches, raw_data, tdc_timestamp,
                                        gdc_timestamp, timer_lsb32);
        sophiread::timedProcessing(batches, raw_data, config);

        // Generate output file name
        std::string output_file =
            fs::path(output_dir) / (tiff_base + "_bin_xxxx.tiff");

        // Create TOF images
        auto tof_images = sophiread::timedCreateTOFImages(
            batches, config.getSuperResolution(), config.getTOFBinEdges(),
            tof_mode);

        // Save TOF images
        sophiread::timedSaveTOFImagingToTIFF(
            output_dir, tof_images, config.getTOFBinEdges(), tiff_base);

        spdlog::info("Processed and saved: {}", output_file);

        // record processed file
        processed_files.insert(entry.path().stem().string());
      } catch (const std::exception& e) {
        spdlog::error("Error processing file {}: {}", entry.path().string(),
                      e.what());
      }
    }
  }

  // Create a string to hold the formatted output
  std::ostringstream oss;
  oss << "Processed files: [";

  // Loop through the set and concatenate elements
  for (auto it = processed_files.begin(); it != processed_files.end(); ++it) {
    if (it != processed_files.begin()) {
      oss << ", ";  // Add a comma before each element except the first
    }
    oss << *it;
  }
  oss << "]";
  spdlog::info(oss.str());
}

void monitor_directory(const std::string& input_dir,
                       const std::string& output_dir,
                       const std::string& tiff_base,
                       const std::string& tof_mode, const IConfig& config,
                       std::unordered_set<std::string>& processed_files,
                       int check_interval) {
  spdlog::info("Starting directory monitoring: {}", input_dir);
  spdlog::info("Check interval: {} seconds", check_interval);

  while (true) {
    // Check for *.nxs.h5 file
    for (const auto& entry : fs::directory_iterator(input_dir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".h5" &&
          entry.path().stem().extension() == ".nxs") {
        spdlog::info("Found *.nxs.h5 file. Stopping monitoring.");
        return;
      }
    }

    // Process any new files
    process_existing_files(input_dir, output_dir, tiff_base, tof_mode, config,
                           processed_files);

    // Wait before next check
    std::this_thread::sleep_for(std::chrono::seconds(check_interval));
  }
}

int main(int argc, char* argv[]) {
  try {
    ProgramOptions options = parse_arguments(argc, argv);

    // Set logging level based on verbose flag
    if (options.debug) {
      spdlog::set_level(spdlog::level::debug);
      spdlog::debug("Debug logging enabled");
    } else if (options.verbose) {
      spdlog::set_level(spdlog::level::info);
    } else {
      spdlog::set_level(spdlog::level::warn);
    }

    spdlog::info("Input directory: {}", options.input_dir);
    spdlog::info("Output directory: {}", options.output_dir);
    spdlog::info("Config file: {}", options.config_file);
    spdlog::info("TIFF base name: {}", options.tiff_base);
    spdlog::info("TOF mode: {}", options.tof_mode);

    // Load configuration
    std::unique_ptr<IConfig> config;
    if (!options.config_file.empty()) {
      spdlog::info("Config file: {}", options.config_file);
      config = std::make_unique<JSONConfigParser>(
          JSONConfigParser::fromFile(options.config_file));
    } else {
      spdlog::info("Using default configuration");
      config =
          std::make_unique<JSONConfigParser>(JSONConfigParser::createDefault());
    }

    spdlog::info("Configuration: {}", config->toString());

    // Process existing files
    std::unordered_set<std::string> processed_files;
    monitor_directory(options.input_dir, options.output_dir, options.tiff_base,
                      options.tof_mode, *config, processed_files,
                      options.check_interval);

  } catch (const std::exception& e) {
    spdlog::error("Error: {}", e.what());
    return 1;
  }

  return 0;
}