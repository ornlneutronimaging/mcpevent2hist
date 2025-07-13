// TDCSophiread Neutron Processing Factory Implementation
// Factory pattern implementation for creating algorithm instances

#include "neutron_processing/neutron_factories.h"

#include <stdexcept>

#include "neutron_processing/basic_neutron_processor.h"
#include "neutron_processing/neutron_processing.h"
#include "neutron_processing/simple_abs_clustering.h"
#include "neutron_processing/simple_centroid_extraction.h"

namespace tdcsophiread {

// HitClusteringFactory implementation
std::unique_ptr<IHitClustering> HitClusteringFactory::create(
    const std::string& algorithm_name, const HitClusteringConfig& config) {
  // Validate configuration first
  config.validate();

  if (algorithm_name == "simple_abs" || algorithm_name == "abs") {
    return std::make_unique<SimpleABSClustering>(config);
  }

  throw std::invalid_argument("Unknown clustering algorithm: " +
                              algorithm_name);
}

std::vector<std::string> HitClusteringFactory::getAvailableAlgorithms() {
  return {"simple_abs", "abs"};
}

std::string HitClusteringFactory::getAlgorithmDescription(
    const std::string& algorithm_name) {
  if (algorithm_name == "simple_abs" || algorithm_name == "abs") {
    return "Age-Based Spatial clustering with bucket pool management";
  }

  throw std::invalid_argument("Unknown clustering algorithm: " +
                              algorithm_name);
}

// NeutronExtractionFactory implementation
std::unique_ptr<INeutronExtraction> NeutronExtractionFactory::create(
    const std::string& algorithm_name, const NeutronExtractionConfig& config) {
  // Validate configuration first
  config.validate();

  if (algorithm_name == "simple_centroid" || algorithm_name == "centroid") {
    return std::make_unique<SimpleCentroidExtraction>(config);
  }

  throw std::invalid_argument("Unknown extraction algorithm: " +
                              algorithm_name);
}

std::vector<std::string> NeutronExtractionFactory::getAvailableAlgorithms() {
  return {"simple_centroid", "centroid"};
}

std::string NeutronExtractionFactory::getAlgorithmDescription(
    const std::string& algorithm_name) {
  if (algorithm_name == "simple_centroid" || algorithm_name == "centroid") {
    return "TOT-weighted centroid calculation for sub-pixel precision";
  }

  throw std::invalid_argument("Unknown extraction algorithm: " +
                              algorithm_name);
}

// NeutronProcessorFactory implementation
std::unique_ptr<INeutronProcessor> NeutronProcessorFactory::create(
    const NeutronProcessingConfig& config) {
  // Validate configuration first
  config.validate();

  // Create TemporalNeutronProcessor if parallel processing is requested
  if (config.temporal.num_workers != 1) {
    return std::make_unique<TemporalNeutronProcessor>(config);
  }

  // Otherwise use single-threaded BasicNeutronProcessor
  return std::make_unique<BasicNeutronProcessor>(config);
}

std::vector<std::string> NeutronProcessorFactory::getAvailableProcessorTypes() {
  return {"basic", "temporal"};
}

std::string NeutronProcessorFactory::getProcessorDescription(
    const std::string& processor_name) {
  if (processor_name == "basic") {
    return "Single-threaded processor combining clustering and extraction";
  }
  if (processor_name == "temporal") {
    return "Parallel temporal processor with worker pool and statistical "
           "batching";
  }

  throw std::invalid_argument("Unknown processor: " + processor_name);
}

}  // namespace tdcsophiread