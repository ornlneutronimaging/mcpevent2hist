/**
 * @file sophiread.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief CLI for reading Timepix3 raw data and parse it into neutron event
 * files and a tiff image (for visual inspection).
 * @date 2024-08-19
 *
 * @copyright Copyright (c) 2024
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <spdlog/spdlog.h>
#include <tbb/tbb.h>
#include <unistd.h>

#include <tiffio.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>

#include "abs.h"
#include "disk_io.h"
#include "tpx3_fast.h"
#include "user_config.h"
#include "json_config_parser.h"
#include "sophiread_core.h"

/**
 * @brief Main function.
 *
 * @param[in] argc
 * @param[in] argv
 * @return int
 */
int main(int argc, char *argv[]) {
  // ----------------------------------
  // processing command line arguments
  // ----------------------------------
  std::string in_tpx3;
  std::string out_hits;
  std::string out_events;
  std::string config_file;
  std::string out_tof_imaging;
  std::string tof_filename_base = "tof_image";
  std::string tof_mode = "neutron"; // Default to neutron-based TOF imaging
  bool debug_logging = false;
  bool verbose = false;
  int opt;

  // help message string
  std::string help_msg = 
    "Usage:\n" +
    std::string(argv[0]) +
    "\n\t[-i input_tpx3] " +
    "\n\t[-H output_hits_HDF5] " +
    "\n\t[-E output_event_HDF5] " +
    "\n\t[-u user_defined_params]" +
    "\n\t[-T tof_imaging_folder]" +
    "\n\t[-f tof_filename_base]" +
    "\n\t[-m tof_mode(hit or neutron)]" +
    "\n\t[-d]" +
    "\n\t[-v]";

  // parse command line arguments
  while ((opt = getopt(argc, argv, "i:H:E:T:f:u:m:dv")) != -1) {
    switch (opt) {
      case 'i':  // input file
        in_tpx3 = optarg;
        break;
      case 'H':  // output hits file (HDF5)
        out_hits = optarg;
        break;
      case 'E':  // output event file
        out_events = optarg;
        break;
      case 'u':  // user-defined params 
        config_file = optarg;
        break;
      case 'T':  // output TOF imaging folder
        out_tof_imaging = optarg;
        break;
      case 'f':  // TOF filename base
        tof_filename_base = optarg;
        break;
      case 'm': // mode for TOF imaging, neutron: neutrons->tiff; hit: hit->tiff
        tof_mode = optarg;
        break;
      case 'd':
        debug_logging = true;
        break;
      case 'v':
        verbose = true;
        break;
      default:
        spdlog::error(help_msg);
        return 1;
    }
  }
  
  // Set Logging
  if (debug_logging) {
      spdlog::set_level(spdlog::level::debug);
      spdlog::debug("Debug logging enabled");
  } else if (verbose) {
      spdlog::set_level(spdlog::level::info);
  } else {
      spdlog::set_level(spdlog::level::warn);
  }

  // -------------
  // sanity check
  // -------------
  // Check 1: determine config file type and parse accordingly
  std::unique_ptr<IConfig> config;
  if (!config_file.empty()) {
    std::string extension = std::filesystem::path(config_file).extension().string();
    if (extension == ".json") {
      try {
        config = std::make_unique<JSONConfigParser>(JSONConfigParser::fromFile(config_file));
        spdlog::info("Using JSON configuration file: {}", config_file);
      } catch (const std::exception& e) {
        spdlog::error("Error parsing JSON configuration file: {}", e.what());
        return 1;
      }
    } else {
      spdlog::warn("Deprecated configuration format detected. Please switch to JSON format in future.");
      spdlog::warn("Support for the old format will be removed in version 4.x");
      config = std::make_unique<UserConfig>(parseUserDefinedConfigurationFile(config_file));
    }
  } else {
    spdlog::info("No configuration file provided. Using default JSON configuration.");
    config = std::make_unique<JSONConfigParser>(JSONConfigParser::createDefault());
  }
  // Check 2: does the TPX3 exists?
  if (in_tpx3.empty()) {
    spdlog::error("Error: no input file specified.");
    spdlog::error(help_msg);
    return 1;
  }
  // Check 3: model valid
  if (tof_mode != "neutron" && tof_mode != "hit") {
    spdlog::error("Invalid TOF mode specified. Use 'neutron' or 'hit'.");
    return 1;
  }
  // recap
  if (verbose) {
    spdlog::info("Recap input:");
    spdlog::info("\tInput file: {}", in_tpx3);
    spdlog::info("\tOutput hits file: {}", out_hits);
    spdlog::info("\tOutput events file: {}", out_events);
    spdlog::info("\tConfiguration: {}", config->toString());
    spdlog::info("\tTOF imaging mode: {}", tof_mode);
    spdlog::info("\tDebug logging: {}", debug_logging ? "Enabled" : "Disabled");
  }

  // --------
  // process 
  // --------
  // raw data --> hits --> neutrons
  auto start = std::chrono::high_resolution_clock::now();
  auto raw_data = sophiread::timedReadDataToCharVec(in_tpx3);
  auto batches = sophiread::timedFindTPX3H(raw_data);
  sophiread::timedLocateTimeStamp(batches, raw_data);
  sophiread::timedProcessing(batches, raw_data, *config);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Total processing time: {} s", elapsed / 1e6);
  // release memory of raw data
  std::vector<char>().swap(raw_data);

  // neutrons/hits --2D hist--> TOF images
  // NOTE: since x,y are int from hit, so the super resolution will produce checked image,
  //       so users should keep sr at 1 for hit->tiff
  std::vector<std::vector<std::vector<unsigned int>>> tof_images;
  if (!out_tof_imaging.empty()) {
    spdlog::debug("start creating tof images");
    tof_images = sophiread::timedCreateTOFImages(batches, config->getSuperResolution(), config->getTOFBinEdges(), tof_mode);
  }

  // -------------
  // Save to Disk
  // -------------
  // save hits to HDF5 file
  if (!out_hits.empty()) {
    sophiread::timedSaveHitsToHDF5(out_hits, batches);
  }

  // save events to HDF5 file
  if (!out_events.empty()) {
    sophiread::timedSaveEventsToHDF5(out_events, batches);
  }

  // Save TOF imaging to TIFF files
  if (!out_tof_imaging.empty()) {
    sophiread::timedSaveTOFImagingToTIFF(out_tof_imaging, tof_images, config->getTOFBinEdges(), tof_filename_base);
  }

  return 0;
}
