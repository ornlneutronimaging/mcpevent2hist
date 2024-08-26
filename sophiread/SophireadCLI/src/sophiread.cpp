/**
 * @file sophiread.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief CLI for reading Timepix3 raw data and parse it into neutron event
 * files and a tiff image (for visual inspection).
 * @date 2024-08-19
 *
 * @copyright Copyright (c) 2024
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#include <spdlog/spdlog.h>
#include <tbb/tbb.h>
#include <tiffio.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "abs.h"
#include "disk_io.h"
#include "json_config_parser.h"
#include "sophiread_core.h"
#include "tpx3_fast.h"
#include "user_config.h"

struct ProgramOptions {
  std::string input_tpx3;
  std::string output_hits;
  std::string output_events;
  std::string config_file;
  std::string output_tof_imaging;
  std::string tof_filename_base = "tof_image";
  std::string tof_mode = "neutron";
  bool debug_logging = false;
  bool verbose = false;
};

void print_usage(const char* program_name) {
  spdlog::info(
      "Usage: {} -i <input_tpx3> -H <output_hits> -E <output_events> [-u "
      "<config_file>] [-T <tof_imaging_folder>] [-f <tof_filename_base>] [-m "
      "<tof_mode>] [-d] [-v]",
      program_name);
  spdlog::info("Options:");
  spdlog::info("  -i <input_tpx3>          Input TPX3 file");
  spdlog::info("  -H <output_hits>         Output hits HDF5 file");
  spdlog::info("  -E <output_events>       Output events HDF5 file");
  spdlog::info(
      "  -u <config_file>         User configuration JSON file (optional)");
  spdlog::info(
      "  -T <tof_imaging_folder>  Output folder for TIFF TOF images "
      "(optional)");
  spdlog::info(
      "  -f <tof_filename_base>   Base name for TIFF files (default: "
      "tof_image)");
  spdlog::info(
      "  -m <tof_mode>            TOF mode: 'hit' or 'neutron' (default: "
      "neutron)");
  spdlog::info("  -d                       Enable debug logging");
  spdlog::info("  -v                       Enable verbose logging");
}

ProgramOptions parse_arguments(int argc, char* argv[]) {
  ProgramOptions options;
  int opt;

  while ((opt = getopt(argc, argv, "i:H:E:u:T:f:m:dv")) != -1) {
    switch (opt) {
      case 'i':
        options.input_tpx3 = optarg;
        break;
      case 'H':
        options.output_hits = optarg;
        break;
      case 'E':
        options.output_events = optarg;
        break;
      case 'u':
        options.config_file = optarg;
        break;
      case 'T':
        options.output_tof_imaging = optarg;
        break;
      case 'f':
        options.tof_filename_base = optarg;
        break;
      case 'm':
        options.tof_mode = optarg;
        break;
      case 'd':
        options.debug_logging = true;
        break;
      case 'v':
        options.verbose = true;
        break;
      default:
        print_usage(argv[0]);
        throw std::runtime_error(std::string("Invalid argument: ") +
                                 static_cast<char>(optopt));
    }
  }

  // Validate required arguments
  if (options.input_tpx3.empty()) {
    print_usage(argv[0]);
    throw std::runtime_error("Missing required arguments");
  }

  // Validate TOF mode
  if (options.tof_mode != "hit" && options.tof_mode != "neutron") {
    throw std::runtime_error("Invalid TOF mode. Use 'hit' or 'neutron'.");
  }

  return options;
}

/**
 * @brief Main function.
 *
 * @param[in] argc
 * @param[in] argv
 * @return int
 */
int main(int argc, char* argv[]) {
  try {
    ProgramOptions options = parse_arguments(argc, argv);

    // Set logging level based on debug and verbose flags
    if (options.debug_logging) {
      spdlog::set_level(spdlog::level::debug);
      spdlog::debug("Debug logging enabled");
    } else if (options.verbose) {
      spdlog::set_level(spdlog::level::info);
    } else {
      spdlog::set_level(spdlog::level::warn);
    }

    spdlog::info("Input file: {}", options.input_tpx3);
    spdlog::info("Output hits file: {}", options.output_hits);
    spdlog::info("Output events file: {}", options.output_events);
    spdlog::info("Configuration file: {}", options.config_file);
    spdlog::info("TOF imaging folder: {}", options.output_tof_imaging);
    spdlog::info("TOF filename base: {}", options.tof_filename_base);
    spdlog::info("TOF mode: {}", options.tof_mode);

    // Load configuration
    std::unique_ptr<IConfig> config;
    if (!options.config_file.empty()) {
      std::string extension =
          std::filesystem::path(options.config_file).extension().string();
      if (extension == ".json") {
        config = std::make_unique<JSONConfigParser>(
            JSONConfigParser::fromFile(options.config_file));
      } else {
        spdlog::warn(
            "Deprecated configuration format detected. Please switch to JSON "
            "format.");
        config = std::make_unique<UserConfig>(
            parseUserDefinedConfigurationFile(options.config_file));
      }
    } else {
      spdlog::info(
          "No configuration file provided. Using default JSON configuration.");
      config =
          std::make_unique<JSONConfigParser>(JSONConfigParser::createDefault());
    }

    spdlog::info("Configuration: {}", config->toString());

    // Process raw data
    auto start = std::chrono::high_resolution_clock::now();
    auto raw_data = sophiread::timedReadDataToCharVec(options.input_tpx3);
    auto batches = sophiread::timedFindTPX3H(raw_data);
    sophiread::timedLocateTimeStamp(batches, raw_data);
    sophiread::timedProcessing(batches, raw_data, *config);
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    spdlog::info("Total processing time: {} s", elapsed / 1e6);

    // Release memory of raw data
    std::vector<char>().swap(raw_data);

    // Generate and save TOF images
    if (!options.output_tof_imaging.empty()) {
      auto tof_images = sophiread::timedCreateTOFImages(
          batches, config->getSuperResolution(), config->getTOFBinEdges(),
          options.tof_mode);
      sophiread::timedSaveTOFImagingToTIFF(options.output_tof_imaging,
                                           tof_images, config->getTOFBinEdges(),
                                           options.tof_filename_base);
    }

    // Save hits and events
    if (!options.output_hits.empty()) {
      sophiread::timedSaveHitsToHDF5(options.output_hits, batches);
    }

    if (!options.output_events.empty()) {
      sophiread::timedSaveEventsToHDF5(options.output_events, batches);
    }

  } catch (const std::exception& e) {
    spdlog::error("Error: {}", e.what());
    return 1;
  }

  return 0;
}
