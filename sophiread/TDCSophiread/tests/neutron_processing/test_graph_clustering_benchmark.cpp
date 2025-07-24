// TDCSophiread Graph Clustering Benchmark
// Compare performance of graph clustering vs ABS clustering

#include <chrono>
#include <iostream>
#include <random>
#include <vector>

#include "neutron_processing/abs_clustering.h"
#include "neutron_processing/graph_clustering.h"
#include "neutron_processing/neutron_config.h"
#include "neutron_processing/neutron_factories.h"
#include "neutron_processing/neutron_processing.h"
// temporal_neutron_processor.h is included via neutron_processing.h
#include "tdc_hit.h"

namespace tdcsophiread {

// Generate synthetic neutron events
std::vector<TDCHit> generateNeutronEvents(size_t num_events,
                                          size_t hits_per_event,
                                          double spatial_spread = 10.0,
                                          double temporal_spread = 50.0) {
  std::vector<TDCHit> hits;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> spatial_dist(-spatial_spread,
                                                spatial_spread);
  std::uniform_real_distribution<> temporal_dist(0, temporal_spread);
  std::uniform_int_distribution<> tot_dist(50, 200);

  for (size_t event = 0; event < num_events; ++event) {
    // Random center for this neutron event
    double center_x = 50 + event * 100;  // Space events apart
    double center_y = 250;
    uint32_t base_tof = event * 5000;  // Temporal separation

    // Generate hits around the center
    for (size_t i = 0; i < hits_per_event; ++i) {
      uint16_t x = static_cast<uint16_t>(center_x + spatial_dist(gen));
      uint16_t y = static_cast<uint16_t>(center_y + spatial_dist(gen));
      uint32_t tof = base_tof + static_cast<uint32_t>(temporal_dist(gen));
      uint16_t tot = static_cast<uint16_t>(tot_dist(gen));

      hits.emplace_back(tof, x, y, 0, tot, 0);
    }
  }

  return hits;
}

void benchmarkClustering(const std::string& algorithm_name,
                         const std::vector<TDCHit>& hits) {
  // Configure neutron processing
  NeutronProcessingConfig config;
  config.clustering.algorithm = algorithm_name;
  config.clustering.abs.radius = 5.0;
  config.clustering.graph.radius = 5.0;
  config.temporal.num_workers = 4;  // Use 4 workers
  config.temporal.min_batch_size = 10000;
  config.temporal.max_batch_size = 50000;

  // Create processor
  auto processor = std::make_unique<TemporalNeutronProcessor>(config);

  // Warm up
  processor->processHits(hits);

  // Benchmark
  const int num_runs = 5;
  double total_time = 0.0;
  size_t total_neutrons = 0;

  for (int run = 0; run < num_runs; ++run) {
    auto start = std::chrono::high_resolution_clock::now();
    auto neutrons = processor->processHits(hits);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    total_time += duration / 1000.0;  // Convert to ms
    total_neutrons += neutrons.size();
  }

  double avg_time = total_time / num_runs;
  double hits_per_sec = (hits.size() * num_runs) / (total_time / 1000.0);
  double avg_neutrons = static_cast<double>(total_neutrons) / num_runs;

  std::cout << "\n" << algorithm_name << " Clustering Results:" << std::endl;
  std::cout << "  Average time: " << avg_time << " ms" << std::endl;
  std::cout << "  Throughput: " << hits_per_sec / 1e6 << " M hits/sec"
            << std::endl;
  std::cout << "  Average neutrons found: " << avg_neutrons << std::endl;
  std::cout << "  Last run statistics:" << std::endl;
  std::cout << "    Processing time: " << processor->getLastProcessingTimeMs()
            << " ms" << std::endl;
  std::cout << "    Hits per second: "
            << processor->getLastHitsPerSecond() / 1e6 << " M hits/sec"
            << std::endl;
}

}  // namespace tdcsophiread

int main(int argc, char* argv[]) {
  using namespace tdcsophiread;

  std::cout << "=== Graph Clustering vs ABS Clustering Benchmark ==="
            << std::endl;

  // Generate test data
  size_t num_events = 1000;
  size_t hits_per_event = 20;

  if (argc > 1) {
    num_events = std::stoul(argv[1]);
  }
  if (argc > 2) {
    hits_per_event = std::stoul(argv[2]);
  }

  std::cout << "\nGenerating " << num_events << " neutron events with "
            << hits_per_event << " hits each..." << std::endl;

  auto hits = generateNeutronEvents(num_events, hits_per_event);
  std::cout << "Total hits: " << hits.size() << std::endl;

  // Benchmark ABS clustering
  benchmarkClustering("abs", hits);

  // Benchmark Graph clustering
  benchmarkClustering("graph", hits);

  // Compare with different event densities
  std::cout << "\n=== Varying Event Density Test ===" << std::endl;

  // Sparse events (well separated)
  auto sparse_hits = generateNeutronEvents(500, 10, 5.0, 20.0);
  std::cout << "\nSparse events (500 events, 10 hits each, tight clusters):"
            << std::endl;
  benchmarkClustering("abs", sparse_hits);
  benchmarkClustering("graph", sparse_hits);

  // Dense events (overlapping)
  auto dense_hits = generateNeutronEvents(200, 50, 15.0, 100.0);
  std::cout << "\nDense events (200 events, 50 hits each, spread clusters):"
            << std::endl;
  benchmarkClustering("abs", dense_hits);
  benchmarkClustering("graph", dense_hits);

  return 0;
}