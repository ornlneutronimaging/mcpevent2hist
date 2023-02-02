/**
 * @brief CLI demo case for using the streaming API to process raw data.
 *
 */
#include <unistd.h>

#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

#include "tpx3.h"

std::mutex mtx;

int main(int argc, char *argv[]) {
  // processing command line arguments
  std::string in_tpx3;
  std::string out_hits;
  std::string out_events;
  bool verbose = false;
  int opt;

  // help message string
  std::string help_msg = "Usage: " + std::string(argv[0]) +
                         " [-i input_tpx3] " + " [-E output_event_HDF5] " +
                         " [-v]";

  // parse command line arguments
  while ((opt = getopt(argc, argv, "i:H:E:v")) != -1) {
    switch (opt) {
      case 'i':  // input file
        in_tpx3 = optarg;
        break;
      case 'E':  // output event file
        out_events = optarg;
        break;
      case 'v':
        verbose = true;
        break;
      default:
        std::cerr << help_msg << std::endl;
        return 1;
    }
  }

  // recap
  if (verbose) {
    std::cout << "Input file: " << in_tpx3 << std::endl;
    std::cout << "Output hits file: " << out_hits << std::endl;
    std::cout << "Output events file: " << out_events << std::endl;
  }

  //
  std::unique_lock<std::mutex> lock(mtx);
  std::condition_variable cv;
  bool read_complete = false;
  std::queue<Hit> hits_queue;
  // child thread: read raw data to queue
  std::thread t1(streamTimepix3RawData, in_tpx3, std::ref(hits_queue),
                 std::ref(lock), std::ref(cv), std::ref(read_complete));
  // main thread: process hits from queue
  std::vector<NeutronEvent> events = clusterStreamTimepix3RawData(
      std::ref(hits_queue), "ABS", std::ref(lock), std::ref(cv),
      std::ref(read_complete), 10000);

  t1.join();

  // print out events
  if (verbose) {
    std::cout << "Found " << events.size() << " events." << std::endl;
  }
  // save events to HDF5 file
  if (!out_events.empty()) {
    saveEventsToHDF5(out_events, events);
  }
  return 0;
}