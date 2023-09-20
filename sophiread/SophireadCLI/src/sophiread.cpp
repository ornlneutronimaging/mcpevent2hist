/**
 * @brief CLI for reading Timepix3 raw data and parse it into neutron event
 * files and a tiff image (for visual inspection).
 *
 */
#include <spdlog/spdlog.h>
#include <tbb/tbb.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

#include "abs.h"
#include "disk_io.h"
#include "tpx3_fast.h"
#include "user_config.h"

/**
 * @brief Timed read raw data to char vector.
 *
 * @param[in] in_tpx3
 * @return std::vector<char>
 */
std::vector<char> timedReadDataToCharVec(const std::string &in_tpx3) {
  auto start = std::chrono::high_resolution_clock::now();
  auto raw_data = readTPX3RawToCharVec(in_tpx3);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Read raw data: {} s", elapsed / 1e6);

  return raw_data;
}

/**
 * @brief Timed find TPX3H.
 *
 * @param[in] rawdata
 * @return std::vector<TPX3>
 */
std::vector<TPX3> timedFindTPX3H(const std::vector<char> &rawdata) {
  auto start = std::chrono::high_resolution_clock::now();
  auto batches = findTPX3H(rawdata);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Locate all headers: {} s", elapsed / 1e6);

  return batches;
}

/**
 * @brief Timed locate timestamp.
 *
 * @param[in, out] batches
 * @param[in] rawdata
 */
void timedLocateTimeStamp(std::vector<TPX3> &batches, const std::vector<char> &rawdata) {
  auto start = std::chrono::high_resolution_clock::now();
  unsigned long tdc_timestamp = 0;
  unsigned long long gdc_timestamp = 0;
  unsigned long timer_lsb32 = 0;
  for (auto &tpx3 : batches) {
    updateTimestamp(tpx3, rawdata, tdc_timestamp, gdc_timestamp, timer_lsb32);
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Locate all timestamps: {} s", elapsed / 1e6);
}

/**
 * @brief Timed hits extraction and clustering via single thread.
 *
 * @param[in, out] batches
 * @param[in] rawdata
 * @param[in] config
 */
void timedProcessing(std::vector<TPX3> &batches, const std::vector<char> &raw_data, const UserConfig &config) {
  auto start = std::chrono::high_resolution_clock::now();
  // Define ABS algorithm with user-defined parameters for each thread
  auto abs_alg_mt =
      std::make_unique<ABS>(config.getABSRadius(), config.getABSMinClusterSize(), config.getABSSpidertimeRange());

  for (auto& tpx3 : batches){
    extractHits(tpx3, raw_data);

    abs_alg_mt->reset();
    abs_alg_mt->set_method("centroid");
    abs_alg_mt->fit(tpx3.hits);

    tpx3.neutrons = abs_alg_mt->get_events(tpx3.hits);
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Process all hits -> neutrons: {} s", elapsed / 1e6);
}


/**
 * @brief Timed hits extraction and clustering via multi-threading.
 *
 * @param[in, out] batches
 * @param[in] rawdata
 * @param[in] config
 */
void timedMultiProcessing(std::vector<TPX3> &batches, const std::vector<char> &raw_data, const UserConfig &config) {
  auto start = std::chrono::high_resolution_clock::now();
  tbb::parallel_for(tbb::blocked_range<size_t>(0, batches.size()), [&](const tbb::blocked_range<size_t> &r) {
    // Define ABS algorithm with user-defined parameters for each thread
    auto abs_alg_mt =
        std::make_unique<ABS>(config.getABSRadius(), config.getABSMinClusterSize(), config.getABSSpidertimeRange());

    for (size_t i = r.begin(); i != r.end(); ++i) {
      auto &tpx3 = batches[i];
      extractHits(tpx3, raw_data);

      abs_alg_mt->reset();
      abs_alg_mt->set_method("centroid");
      abs_alg_mt->fit(tpx3.hits);

      tpx3.neutrons = abs_alg_mt->get_events(tpx3.hits);
    }
  });
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Process all hits -> neutrons: {} s", elapsed / 1e6);
}

/**
 * @brief Timed save hits to HDF5.
 *
 * @param[in] out_hits
 * @param[in] hits
 */
void timedSaveHitsToHDF5(const std::string &out_hits, std::vector<TPX3> &batches) {
  auto start = std::chrono::high_resolution_clock::now();
  // move all hits into a single vector
  std::vector<Hit> hits;
  for (const auto &tpx3 : batches) {
    auto tpx3_hits = tpx3.hits;
    hits.insert(hits.end(), tpx3_hits.begin(), tpx3_hits.end());
  }
  // save hits to HDF5 file
  saveHitsToHDF5(out_hits, hits);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Save hits to HDF5: {} s", elapsed / 1e6);
}

void timedSaveEventsToHDF5(const std::string &out_events, std::vector<TPX3> &batches) {
  auto start = std::chrono::high_resolution_clock::now();
  // move all events into a single vector
  std::vector<Neutron> events;
  for (const auto &tpx3 : batches) {
    auto tpx3_events = tpx3.neutrons;
    events.insert(events.end(), tpx3_events.begin(), tpx3_events.end());
  }
  // save events to HDF5 file
  saveNeutronToHDF5(out_events, events);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  
  spdlog::info("Save events to HDF5: {} s", elapsed / 1e6);
}

void printHitsAndEvents(std::vector<TPX3> &batches){
  std::vector<Hit> hits;
  std::vector<Neutron> events;
  for (const auto &tpx3 : batches){
    auto tpx3_hits = tpx3.hits;
    hits.insert(hits.end(), tpx3_hits.begin(), tpx3_hits.end());

    auto tpx3_events = tpx3.neutrons;
    events.insert(events.end(), tpx3_events.begin(), tpx3_events.end());
  }
  spdlog::info("Number of hits: {}", hits.size());
  spdlog::info("Number of events: {}", events.size());
}

/**
 * @brief Main function.
 *
 * @param[in] argc
 * @param[in] argv
 * @return int
 */
int main(int argc, char *argv[]) {
  // processing command line arguments
  std::string in_tpx3;
  std::string out_hits;
  std::string out_events;
  std::string user_defined_params;
  bool verbose = false;
  int opt;

  // help message string
  std::string help_msg = "Usage: " + std::string(argv[0]) + " [-i input_tpx3] " + " [-H output_hits_HDF5] " +
                         " [-E output_event_HDF5] " + " [-u user_defined_params]" + " [-v]";

  // parse command line arguments
  while ((opt = getopt(argc, argv, "i:H:E:u:v")) != -1) {
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
        user_defined_params = optarg;
        break;
      case 'v':
        verbose = true;
        break;
      default:
        spdlog::error(help_msg);
        return 1;
    }
  }

  // If provided user-defined params, parse it
  UserConfig config;
  if (!user_defined_params.empty()) {
    config = parseUserDefinedConfigurationFile(user_defined_params);
  }

  // read raw data
  if (in_tpx3.empty()) {
    spdlog::error("Error: no input file specified.");
    spdlog::error(help_msg);
    return 1;
  }

  // process file to hits
  auto start = std::chrono::high_resolution_clock::now();
  auto raw_data = timedReadDataToCharVec(in_tpx3);
  auto batches = timedFindTPX3H(raw_data);
  timedLocateTimeStamp(batches, raw_data);
  timedProcessing(batches, raw_data, config);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  spdlog::info("Total processing time: {} s", elapsed / 1e6);

  // release memory of raw data
  std::vector<char>().swap(raw_data);

  // save hits to HDF5 file
  if (!out_hits.empty()) {
    timedSaveHitsToHDF5(out_hits, batches);
  }

  // save events to HDF5 file
  if (!out_events.empty()) {
    timedSaveEventsToHDF5(out_events, batches);
  }

  // recap
  if (verbose) {
    spdlog::info("User-defined configuration: {}", config.toString());
    spdlog::info("Input file: {}", in_tpx3);
    spdlog::info("Output hits file: {}", out_hits);
    spdlog::info("Output events file: {}", out_events);
    printHitsAndEvents(batches);
  }

  return 0;
}
