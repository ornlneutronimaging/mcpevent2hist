/**
 * @brief CLI for reading Timepix3 raw data and parse it into neutron event
 * files and a tiff image (for visual inspection).
 *
 */
#include <fstream>
#include <iostream>
#include <unistd.h>

#include "tpx3.h"

int main(int argc, char *argv[]) {
  auto input_tpx3 = std::string(argv[1]);

  // processing command line arguments
  std::string in_tpx3;
  std::string out_hits;
  bool verbose = false;

  int opt;
  while ((opt = getopt(argc, argv, "i:o:v")) != -1) {
    switch (opt) {
    case 'i':
      in_tpx3 = optarg;
      break;
    case 'o':
      out_hits = optarg;
      break;
    case 'v':
      verbose = true;
      break;
    default:
      std::cerr << "Usage: " << argv[0] << " [-i input_tpx3] [-o output_hits] [-v]" << std::endl;
      return 1;
    }
  }

  // verbose output
  if (verbose) {
    std::cout << "Input file: " << in_tpx3 << std::endl;
    std::cout << "Output hits: " << out_hits << std::endl;
  }

  // read raw data
  if (in_tpx3.empty()) {
    std::cerr << "Error: no input file specified." << std::endl;
    return 1;
  }
  auto hits = readTimepix3RawData(in_tpx3);

  // write hits to file
  // TODO: convert to output neutron events (x, y, tof) onces clustering and
  //       peak fitting is implemented
  if (!out_hits.empty()){
    std::ofstream xy_file(out_hits);
    for (auto hit : hits) {
      xy_file << hit.toString() << std::endl;
    }
  }

  // write image to file
  // TODO: implement
  return 0;
}
