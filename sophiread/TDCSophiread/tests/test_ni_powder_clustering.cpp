// Test Ni powder clustering performance directly in C++
// This isolates whether the hang is in C++ or Python bindings

#include <chrono>
#include <iostream>
#include <vector>

#include "neutron_processing/neutron_processing.h"
#include "tdc_detector_config.h"
#include "tdc_processor.h"

using namespace tdcsophiread;

int main(int argc, char* argv[]) {
  std::string ni_file =
      "data/"
      "Run_8217_April25_2025_Ni_Powder_MCP_TPX3_0_8C_1_9_AngsMin_serval_000000."
      "tpx3";

  if (argc > 1) {
    ni_file = argv[1];
  }

  std::cout << "=== C++ Ni Powder Clustering Test ===" << std::endl;
  std::cout << "File: " << ni_file << std::endl;

  // Step 1: Process TPX3 file to hits
  std::cout << "\n1. Processing TPX3 file to hits..." << std::endl;
  auto start = std::chrono::high_resolution_clock::now();

  // Create processor with VENUS defaults
  auto detector_config = DetectorConfig::venusDefaults();
  TDCProcessor processor(detector_config);

  // Process file with default 512MB chunks, single-threaded
  auto hits = processor.processFile(ni_file, 512, false, 0);

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "   Extracted " << hits.size() << " hits in " << duration.count()
            << " ms" << std::endl;
  std::cout << "   Rate: " << (hits.size() / (duration.count() / 1000.0) / 1e6)
            << " M hits/sec" << std::endl;

  // Step 2: Configure neutron processing
  std::cout << "\n2. Configuring neutron processing..." << std::endl;
  auto config = NeutronProcessingConfig::venusDefaults();

  // Match notebook settings
  config.extraction.min_tot_threshold = 0;       // Disable TOT filtering
  config.temporal.enable_deduplication = false;  // Disable deduplication

  std::cout << "   Clustering algorithm: " << config.clustering.algorithm
            << std::endl;
  std::cout << "   Min TOT threshold: " << config.extraction.min_tot_threshold
            << std::endl;
  std::cout << "   Deduplication: "
            << (config.temporal.enable_deduplication ? "enabled" : "disabled")
            << std::endl;

  // Step 3: Process hits to neutrons
  std::cout << "\n3. Processing hits to neutrons..." << std::endl;
  std::cout << "   Input: " << hits.size() << " hits" << std::endl;

  start = std::chrono::high_resolution_clock::now();

  try {
    TemporalNeutronProcessor processor(config);

    // Process with progress updates
    std::cout << "   Starting temporal processing..." << std::endl;
    auto neutrons = processor.processHits(hits);

    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\n✅ SUCCESS!" << std::endl;
    std::cout << "   Output: " << neutrons.size() << " neutrons" << std::endl;
    std::cout << "   Time: " << duration.count() << " ms" << std::endl;
    std::cout << "   Rate: "
              << (hits.size() / (duration.count() / 1000.0) / 1e6)
              << " M hits/sec" << std::endl;
    std::cout << "   Efficiency: " << (100.0 * neutrons.size() / hits.size())
              << "%" << std::endl;

    // Get statistics
    std::cout << "\n4. Processing statistics:" << std::endl;
    std::cout << "   Last processing time: "
              << processor.getLastProcessingTimeMs() << " ms" << std::endl;
    std::cout << "   Hits per second: " << processor.getLastHitsPerSecond()
              << std::endl;
    std::cout << "   Neutron efficiency: "
              << processor.getLastNeutronEfficiency() << std::endl;

  } catch (const std::exception& e) {
    std::cout << "\n❌ ERROR: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "\n=== Test Complete ===" << std::endl;
  return 0;
}