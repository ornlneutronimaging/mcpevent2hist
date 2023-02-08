/**
 * @brief CLI for reading Timepix3 raw data and parse it into neutron event
 * files and a tiff image (for visual inspection).
 *
 */
#include <unistd.h>

#include <fstream>
#include <iostream>

#include "abs.h"
#include "tpx3.h"

int main(int argc, char *argv[]) {
  // processing command line arguments
  std::string in_tpx3;
  std::string out_hits;
  std::string out_events;
  bool verbose = false;
  int opt;

  // help message string
  std::string help_msg = "Usage: " + std::string(argv[0]) +
                         " [-i input_tpx3] " + " [-H output_hits_HDF5] " +
                         " [-E output_event_HDF5] " + " [-v]";

  // parse command line arguments
  while ((opt = getopt(argc, argv, "i:H:E:v")) != -1) {
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

  // read raw data
  if (in_tpx3.empty()) {
    std::cerr << "Error: no input file specified." << std::endl;
    std::cerr << help_msg << std::endl;
    return 1;
  }
  auto hits = readTimepix3RawData(in_tpx3);

  // clustering and fitting
  ClusteringAlgorithm *alg = new ABS(5.0);
  alg->set_method("fast_gaussian");
  alg->fit(hits);
  auto labels = alg->get_cluster_labels();
  // print out labeled hits
  if (verbose) {
    std::cout << "Found " << labels.size() << " hits." << std::endl;
  }
  // Save labeled hits to HDF5 file
  if (!out_hits.empty()) {
    saveHitsToHDF5(out_hits, hits, labels);
  }

  // generate events
  auto events = alg->get_events(hits);
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
