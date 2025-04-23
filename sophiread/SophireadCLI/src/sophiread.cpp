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
#include <H5Epublic.h>
#include <spdlog/spdlog.h>
#include <tbb/tbb.h>
#include <tiffio.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
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
  std::string spectra_filen = "Spectra";
  std::string timing_mode = "gdc";    // Default is GDC mode
  size_t chunk_size = 5ULL * 1024 * 1024 * 1024;  // Default 5GB
  bool debug_logging = false;
  bool verbose = false;
};

/**
 * @brief Print usage information.
 *
 * @param[in] program_name
 */
void print_usage(const char* program_name) {
  spdlog::info(
      "Usage: {} -i <input_tpx3> -H <output_hits> -E <output_events> [-u "
      "<config_file>] [-T <tof_imaging_folder>] [-f <tof_filename_base>] [-m "
      "<tof_mode>] [-t <timing_mode>] [-d] [-v]",
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
  spdlog::info("  -t <timing_mode>         Timing mode: 'gdc' or 'tdc' (default: gdc)");
  spdlog::info("  -s <spectra_filename>    Output filename for spectra");
  spdlog::info("  -c <chunk_size>          Chunk size in MB (default: 5120)");
  spdlog::info("  -d                       Enable debug logging");
  spdlog::info("  -v                       Enable verbose logging");
}

/**
 * @brief Parse command line arguments.
 *
 * @param[in] argc
 * @param[in] argv
 */
ProgramOptions parse_arguments(int argc, char* argv[]) {
  ProgramOptions options;
  int opt;

  while ((opt = getopt(argc, argv, "i:H:E:u:T:f:m:t:s:c:dv")) != -1) {
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
      case 't':
        options.timing_mode = optarg;
        break;
      case 's':
        options.spectra_filen = optarg;
        break;
      case 'c':
        options.chunk_size = static_cast<size_t>(std::stoull(optarg)) * 1024 *
                             1024;  // Convert MB to bytes
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
  
  // Validate timing mode
  if (options.timing_mode != "gdc" && options.timing_mode != "tdc") {
    throw std::runtime_error("Invalid timing mode. Use 'gdc' or 'tdc'.");
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
    // Turn off HDF5 error printing
    // NOTE: we handle the errors ourselves
    H5Eset_auto(H5E_DEFAULT, NULL, NULL);

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
    spdlog::info("Timing mode: {}", options.timing_mode);    
    spdlog::info("Chunk size: {} MB", options.chunk_size / (1024 * 1024));

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

    TPX3FileReader fileReader(options.input_tpx3);

    auto start = std::chrono::high_resolution_clock::now();

    // Initialize HDF5 files for hits and events (if needed)
    H5::H5File hitsFile, eventsFile;
    if (!options.output_hits.empty()) {
      hitsFile = H5::H5File(options.output_hits, H5F_ACC_TRUNC);
    }
    if (!options.output_events.empty()) {
      eventsFile = H5::H5File(options.output_events, H5F_ACC_TRUNC);
    }

    // Initialize TOF images if needed
    std::vector<std::vector<std::vector<unsigned int>>> tof_images;
    const bool needs_tof_images =
        !options.output_tof_imaging.empty() || !options.spectra_filen.empty();
    if (needs_tof_images) {
      tof_images = sophiread::initializeTOFImages(config->getSuperResolution(),
                                                  config->getTOFBinEdges());
    }

    unsigned long tdc_timestamp = 0;
    unsigned long long gdc_timestamp = 0;
    unsigned long timer_lsb32 = 0;

    const size_t totalSize = fileReader.getTotalSize();
    size_t processedSize = 0;

    spdlog::info("Starting chunk-based processing of file: {}",
                 options.input_tpx3);
    spdlog::info("Chunk size: {} MB", options.chunk_size / (1024 * 1024));

    int chunkCounter = 0;
    uint64_t totalHits = 0;
    uint64_t totalNeutrons = 0;

    while (!fileReader.isEOF()) {
      try {
        auto chunk = fileReader.readChunk(options.chunk_size);
        if (chunk.empty()) break;

        // report timing info
        spdlog::info("TDC timestamp: {}", tdc_timestamp);
        spdlog::info("GDC timestamp: {}", gdc_timestamp);
        spdlog::info("Timer LSB32: {}", timer_lsb32);

        auto batches = sophiread::timedFindTPX3H(chunk);
        // GDC route
        if (options.timing_mode == "gdc") {
          spdlog::info("Using GDC mode for timestamp processing");
          // update timestamp
          sophiread::timedLocateTimeStamp(batches, chunk, tdc_timestamp,
                                          gdc_timestamp, timer_lsb32);
          // extract hits and neutrons
          sophiread::timedProcessing(batches, chunk, *config, true);
        } else if (options.timing_mode == "tdc") {
          // TDC route
          spdlog::info("Using TDC mode for timestamp processing");
          sophiread::timedLocateTimeStamp(batches, chunk, tdc_timestamp);
          sophiread::timedProcessing(batches, chunk, *config, false);
        } else {
          throw std::runtime_error("Invalid timing mode. Use 'gdc' or 'tdc'.");
        }
        // sophiread::timedLocateTimeStamp(batches, chunk, tdc_timestamp,
        //                                 gdc_timestamp, timer_lsb32);
        // sophiread::timedProcessing(batches, chunk, *config);

        // Process hits and neutrons
        for (const auto& batch : batches) {
          // Append hits to HDF5 file
          if (!options.output_hits.empty()) {
            spdlog::debug("Appending hits to HDF5 file");
            appendHitsToHDF5Extendible(hitsFile, batch.hits);
          }

          // Append neutrons to HDF5 file
          if (!options.output_events.empty()) {
            spdlog::debug("Appending neutrons to HDF5 file");
            appendNeutronsToHDF5Extendible(eventsFile, batch.neutrons);
          }

          // Update TOF images
          if (needs_tof_images) {
            spdlog::debug("Updating TOF images");
            sophiread::updateTOFImages(
                tof_images, batch, config->getSuperResolution(),
                config->getTOFBinEdges(), options.tof_mode);
          }

          // Update counters
          totalHits += batch.hits.size();
          totalNeutrons += batch.neutrons.size();

          // Print debug information
          spdlog::debug("Chunk {}: Hits: {}, Neutrons: {}", chunkCounter,
                        batch.hits.size(), batch.neutrons.size());
          spdlog::debug("Total Hits: {}, Total Neutrons: {}", totalHits,
                        totalNeutrons);
        }

        // Update progress
        spdlog::debug("Processed chunk {}: {} bytes", chunkCounter,
                      chunk.size());
        processedSize += chunk.size();
        float progress = static_cast<float>(processedSize) / totalSize * 100.0f;
        spdlog::info("Progress: {:.2f}%", progress);

        // Update counters
        chunkCounter++;

        // Clear memory
        std::vector<TPX3>().swap(batches);
        std::vector<char>().swap(chunk);
      } catch (const std::exception& e) {
        spdlog::error("Error processing chunk: {}", e.what());
      }
    }

    // Close HDF5 files
    if (!options.output_hits.empty()) {
      hitsFile.close();
    }
    if (!options.output_events.empty()) {
      eventsFile.close();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    spdlog::info("Total processing time: {} s", elapsed / 1e6);
    spdlog::info("Total chunks processed: {}", chunkCounter);
    spdlog::info("Total hits: {}", totalHits);
    spdlog::info("Total neutrons: {}", totalNeutrons);

    // Save TOF images if needed
    if (!options.output_tof_imaging.empty()) {
      spdlog::info("Saving TOF imaging to TIFF: {}",
                   options.output_tof_imaging);
      sophiread::timedSaveTOFImagingToTIFF(options.output_tof_imaging,
                                           tof_images, config->getTOFBinEdges(),
                                           options.tof_filename_base);
    }

    // Save spectra if needed
    if (!options.spectra_filen.empty()) {
      spdlog::info("Saving spectra to file: {}", options.spectra_filen);
      std::string spectral_filename = options.spectra_filen + ".txt";
      std::vector<uint64_t> spectral_counts =
          sophiread::calculateSpectralCounts(tof_images);
      sophiread::writeSpectralFile(spectral_filename, spectral_counts,
                                   config->getTOFBinEdges());
    }

  } catch (const std::exception& e) {
    spdlog::error("Error: {}", e.what());
    return 1;
  }
  return 0;
}
