// TDCSophiread Python Bindings
// Exposes high-performance TDC processor to Python via pybind11

#include <pybind11/chrono.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <fstream>
#include <stdexcept>

#include "tdc_cluster_processor.h"
#include "tdc_clustering_config.h"
#include "tdc_detector_config.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"
#include "tdc_processor.h"

namespace py = pybind11;

// Make vector of TDCHit opaque for efficient transfer
PYBIND11_MAKE_OPAQUE(std::vector<tdcsophiread::TDCHit>);
PYBIND11_MAKE_OPAQUE(std::vector<tdcsophiread::TDCNeutron>);

namespace tdcsophiread {

// Custom exception types for better error handling
class TDCProcessingError : public std::runtime_error {
 public:
  explicit TDCProcessingError(const std::string& msg)
      : std::runtime_error(msg) {}
};

class TDCFileError : public std::runtime_error {
 public:
  explicit TDCFileError(const std::string& msg) : std::runtime_error(msg) {}
};

class TDCConfigError : public std::runtime_error {
 public:
  explicit TDCConfigError(const std::string& msg) : std::runtime_error(msg) {}
};

// Progress callback wrapper for Python functions
class ProgressCallback {
 public:
  ProgressCallback() = default;
  explicit ProgressCallback(py::function callback) : callback_(callback) {}

  void operator()(double progress, const std::string& message = "") const {
    if (callback_) {
      callback_(progress, message);
    }
  }

  bool is_valid() const { return callback_.is_none() == false; }

 private:
  py::function callback_;
};

// Helper function to convert hits to numpy arrays
py::dict hits_to_numpy(const std::vector<TDCHit>& hits) {
  size_t n = hits.size();

  // Create numpy arrays for each field
  auto x = py::array_t<uint16_t>(n);
  auto y = py::array_t<uint16_t>(n);
  auto tof = py::array_t<uint32_t>(n);
  auto tot = py::array_t<uint16_t>(n);
  auto chip_id = py::array_t<uint8_t>(n);
  auto timestamp = py::array_t<uint32_t>(n);

  // Get raw pointers for fast access
  auto x_ptr = static_cast<uint16_t*>(x.mutable_unchecked<1>().mutable_data(0));
  auto y_ptr = static_cast<uint16_t*>(y.mutable_unchecked<1>().mutable_data(0));
  auto tof_ptr =
      static_cast<uint32_t*>(tof.mutable_unchecked<1>().mutable_data(0));
  auto tot_ptr =
      static_cast<uint16_t*>(tot.mutable_unchecked<1>().mutable_data(0));
  auto chip_id_ptr =
      static_cast<uint8_t*>(chip_id.mutable_unchecked<1>().mutable_data(0));
  auto timestamp_ptr =
      static_cast<uint32_t*>(timestamp.mutable_unchecked<1>().mutable_data(0));

  // Copy data
  for (size_t i = 0; i < n; ++i) {
    x_ptr[i] = hits[i].x;
    y_ptr[i] = hits[i].y;
    tof_ptr[i] = hits[i].tof;
    tot_ptr[i] = hits[i].tot;
    chip_id_ptr[i] = hits[i].chip_id;
    timestamp_ptr[i] = hits[i].timestamp;
  }

  // Return as dictionary
  py::dict result;
  result["x"] = x;
  result["y"] = y;
  result["tof"] = tof;
  result["tot"] = tot;
  result["chip_id"] = chip_id;
  result["timestamp"] = timestamp;

  return result;
}

// Helper function to convert neutrons to numpy arrays
py::dict neutrons_to_numpy(const std::vector<TDCNeutron>& neutrons) {
  size_t n = neutrons.size();

  // Create numpy arrays for each field
  auto x = py::array_t<double>(n);
  auto y = py::array_t<double>(n);
  auto tof = py::array_t<uint32_t>(n);
  auto tot = py::array_t<uint16_t>(n);
  auto n_hits = py::array_t<uint16_t>(n);
  auto chip_id = py::array_t<uint8_t>(n);

  // Get raw pointers for fast access
  auto x_ptr = static_cast<double*>(x.mutable_unchecked<1>().mutable_data(0));
  auto y_ptr = static_cast<double*>(y.mutable_unchecked<1>().mutable_data(0));
  auto tof_ptr =
      static_cast<uint32_t*>(tof.mutable_unchecked<1>().mutable_data(0));
  auto tot_ptr =
      static_cast<uint16_t*>(tot.mutable_unchecked<1>().mutable_data(0));
  auto n_hits_ptr =
      static_cast<uint16_t*>(n_hits.mutable_unchecked<1>().mutable_data(0));
  auto chip_id_ptr =
      static_cast<uint8_t*>(chip_id.mutable_unchecked<1>().mutable_data(0));

  // Copy data
  for (size_t i = 0; i < n; ++i) {
    x_ptr[i] = neutrons[i].x;
    y_ptr[i] = neutrons[i].y;
    tof_ptr[i] = neutrons[i].tof;
    tot_ptr[i] = neutrons[i].tot;
    n_hits_ptr[i] = neutrons[i].n_hits;
    chip_id_ptr[i] = neutrons[i].chip_id;
  }

  // Return as dictionary
  py::dict result;
  result["x"] = x;
  result["y"] = y;
  result["tof"] = tof;
  result["tot"] = tot;
  result["n_hits"] = n_hits;
  result["chip_id"] = chip_id;

  return result;
}

// Streaming processor with progress callbacks and memory management
class TDCStreamProcessor {
 public:
  explicit TDCStreamProcessor(const DetectorConfig& config)
      : processor_(config) {}

  // Process file in chunks with progress callback (returns list of chunk
  // results)
  py::list process_file_stream(const std::string& file_path,
                               size_t chunk_size_mb = 512,
                               py::object progress_callback = py::none()) {
    ProgressCallback callback;
    if (!progress_callback.is_none()) {
      callback = ProgressCallback(progress_callback.cast<py::function>());
    }

    // Get file size for progress tracking
    std::ifstream file(file_path, std::ios::ate | std::ios::binary);
    if (!file.good()) {
      throw TDCFileError("Cannot open file: " + file_path);
    }
    size_t file_size = file.tellg();
    file.close();

    size_t chunk_size_bytes = chunk_size_mb * 1024 * 1024;
    size_t processed_bytes = 0;
    py::list results;

    if (callback.is_valid()) {
      callback(0.0, "Starting file processing...");
    }

    while (processed_bytes < file_size) {
      size_t remaining = file_size - processed_bytes;
      size_t chunk_size = std::min(chunk_size_bytes, remaining);

      size_t actual_processed = 0;
      auto hits = processor_.processChunk(file_path, processed_bytes,
                                          chunk_size, actual_processed);

      if (actual_processed == 0) {
        break;  // No more data to process
      }

      processed_bytes += actual_processed;
      double progress = static_cast<double>(processed_bytes) / file_size;

      if (callback.is_valid()) {
        std::string msg = "Processed " + std::to_string(hits.size()) + " hits";
        callback(progress, msg);
      }

      // Add chunk results to list
      results.append(hits_to_numpy(hits));
    }

    if (callback.is_valid()) {
      callback(1.0, "Processing complete");
    }

    return results;
  }

  // Context manager support
  TDCStreamProcessor& __enter__() { return *this; }
  void __exit__(py::object /* exc_type */, py::object /* exc_value */,
                py::object /* traceback */) {
    // Cleanup if needed
  }

 private:
  TDCProcessor processor_;
};

PYBIND11_MODULE(_core, m) {
  m.doc() = "High-performance TDC-only TPX3 data processor";

  // Version information
  m.attr("__version__") = PROJECT_VERSION;

  // Register custom exception types
  py::register_exception<TDCProcessingError>(m, "TDCProcessingError");
  py::register_exception<TDCFileError>(m, "TDCFileError");
  py::register_exception<TDCConfigError>(m, "TDCConfigError");

  // ChipTransform class
  py::class_<ChipTransform>(m, "ChipTransform")
      .def(py::init<>(), "Create identity transformation")
      .def(py::init<double, double, double, double, double, double>(),
           py::arg("a"), py::arg("b"), py::arg("tx"), py::arg("c"),
           py::arg("d"), py::arg("ty"), "Create affine transformation matrix")
      .def("apply", &ChipTransform::apply, py::arg("x"), py::arg("y"),
           "Apply transformation to coordinates")
      .def_property_readonly(
          "matrix",
          [](const ChipTransform& self) {
            py::array_t<double> arr({2, 3});
            auto ptr = static_cast<double*>(
                arr.mutable_unchecked<2>().mutable_data(0, 0));
            for (int i = 0; i < 2; ++i) {
              for (int j = 0; j < 3; ++j) {
                ptr[i * 3 + j] = self.matrix[i][j];
              }
            }
            return arr;
          },
          "Get transformation matrix as 2x3 numpy array");

  // DetectorConfig class
  py::class_<DetectorConfig>(m, "DetectorConfig")
      .def_static("venus_defaults", &DetectorConfig::venusDefaults,
                  "Create VENUS detector default configuration")
      .def_static("from_file", &DetectorConfig::fromFile,
                  py::arg("config_path"), "Load configuration from JSON file")
      .def_static(
          "from_json",
          [](const py::object& config_obj) {
            // Convert Python object to nlohmann::json string then parse
            py::module json_module = py::module::import("json");
            py::str json_str = json_module.attr("dumps")(config_obj);
            nlohmann::json json_config =
                nlohmann::json::parse(json_str.cast<std::string>());
            return DetectorConfig::fromJson(json_config);
          },
          py::arg("config"),
          "Load configuration from dictionary or JSON-compatible object")
      .def("get_tdc_frequency", &DetectorConfig::getTdcFrequency,
           "Get TDC frequency in Hz")
      .def("is_missing_tdc_correction_enabled",
           &DetectorConfig::isMissingTdcCorrectionEnabled,
           "Check if missing TDC correction is enabled")
      .def("get_chip_size_x", &DetectorConfig::getChipSizeX,
           "Get chip size in X dimension")
      .def("get_chip_size_y", &DetectorConfig::getChipSizeY,
           "Get chip size in Y dimension")
      .def("map_chip_to_global", &DetectorConfig::mapChipToGlobal,
           py::arg("chip_id"), py::arg("local_x"), py::arg("local_y"),
           "Map chip coordinates to global detector coordinates")
      .def("get_chip_transform", &DetectorConfig::getChipTransform,
           py::arg("chip_id"), "Get transformation matrix for specific chip",
           py::return_value_policy::reference_internal)
      .def("set_chip_transform", &DetectorConfig::setChipTransform,
           py::arg("chip_id"), py::arg("transform"),
           "Set transformation matrix for specific chip");

  // TDCHit structure (for individual access if needed)
  py::class_<TDCHit>(m, "TDCHit")
      .def(py::init<>())
      .def_readwrite("x", &TDCHit::x, "Global X coordinate")
      .def_readwrite("y", &TDCHit::y, "Global Y coordinate")
      .def_readwrite("tof", &TDCHit::tof, "Time-of-flight (25ns units)")
      .def_readwrite("tot", &TDCHit::tot, "Time-over-threshold")
      .def_readwrite("chip_id", &TDCHit::chip_id, "Chip ID (0-3)")
      .def_readwrite("timestamp", &TDCHit::timestamp,
                     "Hit timestamp (25ns units)");

  // Bind vector of TDCHit for efficient operations
  py::bind_vector<std::vector<TDCHit>>(m, "TDCHitVector");

  // TDCNeutron structure for neutron events
  py::class_<TDCNeutron>(m, "TDCNeutron")
      .def(py::init<>())
      .def_readwrite("x", &TDCNeutron::x, "Sub-pixel X coordinate")
      .def_readwrite("y", &TDCNeutron::y, "Sub-pixel Y coordinate")
      .def_readwrite("tof", &TDCNeutron::tof, "Time-of-flight (25ns units)")
      .def_readwrite("tot", &TDCNeutron::tot, "Combined time-over-threshold")
      .def_readwrite("n_hits", &TDCNeutron::n_hits, "Number of hits in cluster")
      .def_readwrite("chip_id", &TDCNeutron::chip_id, "Chip ID (0-3)")
      .def("getTOFNanoseconds", &TDCNeutron::getTOFNanoseconds,
           "Get TOF in nanoseconds")
      .def("getTOFMilliseconds", &TDCNeutron::getTOFMilliseconds,
           "Get TOF in milliseconds");

  // Bind vector of TDCNeutron for efficient operations
  py::bind_vector<std::vector<TDCNeutron>>(m, "TDCNeutronVector");

  // ABSConfig for ABS clustering algorithm
  py::class_<ABSConfig>(m, "ABSConfig")
      .def(py::init<>())
      .def_readwrite("radius", &ABSConfig::radius,
                     "Spatial clustering radius in pixels")
      .def_readwrite("min_cluster_size", &ABSConfig::min_cluster_size,
                     "Minimum hits for valid cluster")
      .def_readwrite("neutron_correlation_window",
                     &ABSConfig::neutron_correlation_window,
                     "Neutron temporal correlation window in nanoseconds")
      .def_readwrite("scan_interval", &ABSConfig::scan_interval,
                     "Scan for aged buckets every N hits");

  // CentroidConfig for centroid peak fitting
  py::class_<CentroidConfig>(m, "CentroidConfig")
      .def(py::init<>())
      .def_readwrite("super_resolution_factor",
                     &CentroidConfig::super_resolution_factor,
                     "Coordinate scaling factor for sub-pixel precision")
      .def_readwrite("weighted_by_tot", &CentroidConfig::weighted_by_tot,
                     "Use TOT weighting for centroid calculation")
      .def_readwrite("min_tot_threshold", &CentroidConfig::min_tot_threshold,
                     "Minimum TOT for hit inclusion");

  // ClusteringConfig - main configuration class
  py::class_<ClusteringConfig>(m, "ClusteringConfig")
      .def_static("venus_defaults", &ClusteringConfig::venusDefaults,
                  "Create VENUS detector default clustering configuration")
      .def_static("from_file", &ClusteringConfig::fromFile,
                  py::arg("config_path"),
                  "Load clustering configuration from JSON file")
      .def_static(
          "from_json",
          [](const py::object& config_obj) {
            py::module json_module = py::module::import("json");
            py::str json_str = json_module.attr("dumps")(config_obj);
            nlohmann::json json_config =
                nlohmann::json::parse(json_str.cast<std::string>());
            return ClusteringConfig::fromJson(json_config);
          },
          py::arg("config"),
          "Load clustering configuration from dictionary or JSON-compatible "
          "object")
      .def_readwrite("clustering_algorithm",
                     &ClusteringConfig::clustering_algorithm,
                     "Clustering algorithm name (e.g., 'abs')")
      .def_readwrite("peak_fitting_algorithm",
                     &ClusteringConfig::peak_fitting_algorithm,
                     "Peak fitting algorithm name (e.g., 'centroid')")
      .def_readwrite("enable_clustering", &ClusteringConfig::enable_clustering,
                     "Enable/disable clustering")
      .def_readwrite("abs", &ClusteringConfig::abs,
                     "ABS algorithm configuration")
      .def_readwrite("centroid", &ClusteringConfig::centroid,
                     "Centroid fitting configuration")
      .def("summary", &ClusteringConfig::summary, "Get configuration summary");

  // TDCClusterProcessor - main clustering interface
  py::class_<TDCClusterProcessor>(m, "TDCClusterProcessor")
      .def(py::init<>(), "Create cluster processor with VENUS defaults")
      .def(py::init<const ClusteringConfig&>(), py::arg("config"),
           "Create cluster processor with custom configuration")

      // Main processing method
      .def("process_hits", &TDCClusterProcessor::processHits, py::arg("hits"),
           "Process hits through clustering pipeline to extract neutrons")

      // Configuration
      .def("configure", &TDCClusterProcessor::configure, py::arg("config"),
           "Update clustering configuration")
      .def("get_configuration", &TDCClusterProcessor::getConfiguration,
           "Get current clustering configuration",
           py::return_value_policy::reference_internal)

      // Performance metrics
      .def("get_last_processing_time_ms",
           &TDCClusterProcessor::getLastProcessingTimeMs,
           "Get processing time for last operation in milliseconds")
      .def("get_last_hits_per_second",
           &TDCClusterProcessor::getLastHitsPerSecond,
           "Get processing rate for last operation")
      .def("get_last_neutron_efficiency",
           &TDCClusterProcessor::getLastNeutronEfficiency,
           "Get neutron efficiency (neutrons/hits ratio)")
      .def("get_last_neutron_count", &TDCClusterProcessor::getLastNeutronCount,
           "Get number of neutrons from last operation")

      // Algorithm info
      .def("get_clustering_algorithm",
           &TDCClusterProcessor::getClusteringAlgorithm,
           "Get current clustering algorithm name")
      .def("get_peak_fitting_algorithm",
           &TDCClusterProcessor::getPeakFittingAlgorithm,
           "Get current peak fitting algorithm name")

      // Utilities
      .def("reset", &TDCClusterProcessor::reset, "Reset processor state")
      .def("get_processing_summary", &TDCClusterProcessor::getProcessingSummary,
           "Get human-readable processing summary");

  // TDCStreamProcessor class - enhanced streaming interface with progress
  // callbacks
  py::class_<TDCStreamProcessor>(m, "TDCStreamProcessor")
      .def(py::init<const DetectorConfig&>(), py::arg("config"),
           "Create streaming processor with detector configuration")
      .def("process_file_stream", &TDCStreamProcessor::process_file_stream,
           py::arg("file_path"), py::arg("chunk_size_mb") = 512,
           py::arg("progress_callback") = py::none(),
           "Process file in chunks with optional progress callback")
      .def("__enter__", &TDCStreamProcessor::__enter__,
           py::return_value_policy::reference_internal)
      .def("__exit__", &TDCStreamProcessor::__exit__);

  // TDCProcessor class - main interface
  py::class_<TDCProcessor>(m, "TDCProcessor")
      .def(py::init<const DetectorConfig&>(), py::arg("config"),
           "Create processor with detector configuration")

      // Single-threaded processing
      .def("process_file", &TDCProcessor::processFile, py::arg("file_path"),
           "Process entire TPX3 file (single-threaded)")

      // Parallel processing
      .def("process_file_parallel", &TDCProcessor::processFileParallel,
           py::arg("file_path"), py::arg("num_threads") = 0,
           "Process entire TPX3 file with TBB parallelization")

      // Chunk processing for large files
      .def(
          "process_chunk",
          [](TDCProcessor& self, const std::string& file_path,
             size_t start_offset, size_t requested_size) {
            size_t actual_processed = 0;
            auto hits = self.processChunk(file_path, start_offset,
                                          requested_size, actual_processed);
            return py::make_tuple(hits, actual_processed);
          },
          py::arg("file_path"), py::arg("start_offset"),
          py::arg("requested_size"),
          "Process a chunk of file, returns (hits, actual_bytes_processed)")

      // Configuration
      .def("set_missing_tdc_correction_enabled",
           &TDCProcessor::setMissingTdcCorrectionEnabled, py::arg("enable"),
           "Enable/disable missing TDC correction")

      // Performance metrics
      .def("get_last_processing_time_ms",
           &TDCProcessor::getLastProcessingTimeMs,
           "Get processing time for last operation in milliseconds")
      .def("get_last_hit_count", &TDCProcessor::getLastHitCount,
           "Get number of hits from last operation")
      .def("get_last_hits_per_second", &TDCProcessor::getLastHitsPerSecond,
           "Get processing rate for last operation")
      .def("get_last_packet_count", &TDCProcessor::getLastPacketCount,
           "Get number of packets processed in last operation");

  // Convenience function to convert hits to numpy arrays
  m.def("hits_to_numpy", &hits_to_numpy, py::arg("hits"),
        "Convert vector of hits to dictionary of numpy arrays",
        py::return_value_policy::move);

  // Convenience function to convert neutrons to numpy arrays
  m.def("neutrons_to_numpy", &neutrons_to_numpy, py::arg("neutrons"),
        "Convert vector of neutrons to dictionary of numpy arrays",
        py::return_value_policy::move);

  // High-level convenience function for simple usage with enhanced error
  // handling
  m.def(
      "process_tpx3",
      [](const std::string& file_path, bool parallel = true,
         size_t num_threads = 0, py::object progress_callback = py::none()) {
        try {
          auto config = DetectorConfig::venusDefaults();
          TDCProcessor processor(config);

          ProgressCallback callback;
          if (!progress_callback.is_none()) {
            callback = ProgressCallback(progress_callback.cast<py::function>());
          }
          if (callback.is_valid()) {
            callback(0.0, "Starting TPX3 processing...");
          }

          std::vector<TDCHit> hits;
          if (parallel) {
            hits = processor.processFileParallel(file_path, num_threads);
          } else {
            hits = processor.processFile(file_path);
          }

          if (callback.is_valid()) {
            callback(1.0, "Processing complete");
          }

          return hits_to_numpy(hits);
        } catch (const std::exception& e) {
          throw TDCProcessingError("Failed to process TPX3 file: " +
                                   std::string(e.what()));
        }
      },
      py::arg("file_path"), py::arg("parallel") = true,
      py::arg("num_threads") = 0, py::arg("progress_callback") = py::none(),
      "Process TPX3 file and return numpy arrays (convenience function with "
      "progress support)");

  // Enhanced streaming function for large files
  m.def(
      "process_tpx3_stream",
      [](const std::string& file_path, size_t chunk_size_mb = 512,
         py::object progress_callback = py::none()) {
        try {
          auto config = DetectorConfig::venusDefaults();
          TDCStreamProcessor processor(config);
          return processor.process_file_stream(file_path, chunk_size_mb,
                                               progress_callback);
        } catch (const std::exception& e) {
          throw TDCProcessingError("Failed to stream TPX3 file: " +
                                   std::string(e.what()));
        }
      },
      py::arg("file_path"), py::arg("chunk_size_mb") = 512,
      py::arg("progress_callback") = py::none(),
      "Process large TPX3 files in chunks with progress tracking");

  // Clustering convenience function - process TPX3 file directly to neutrons
  m.def(
      "process_tpx3_to_neutrons",
      [](const std::string& file_path, bool parallel = true,
         size_t num_threads = 0, py::object clustering_config = py::none()) {
        try {
          // Get detector configuration
          auto detector_config = DetectorConfig::venusDefaults();

          // Get clustering configuration
          ClusteringConfig cluster_config;
          if (clustering_config.is_none()) {
            cluster_config = ClusteringConfig::venusDefaults();
          } else if (py::isinstance<ClusteringConfig>(clustering_config)) {
            cluster_config = clustering_config.cast<ClusteringConfig>();
          } else {
            throw TDCConfigError("Invalid clustering configuration type");
          }

          // Process TPX3 file to hits
          TDCProcessor processor(detector_config);
          std::vector<TDCHit> hits;
          if (parallel) {
            hits = processor.processFileParallel(file_path, num_threads);
          } else {
            hits = processor.processFile(file_path);
          }

          // Cluster hits into neutrons - now memory-optimized internally
          TDCClusterProcessor cluster_processor(cluster_config);
          auto neutrons = cluster_processor.processHits(hits);

          return neutrons_to_numpy(neutrons);
        } catch (const std::exception& e) {
          throw TDCProcessingError("Failed to process TPX3 to neutrons: " +
                                   std::string(e.what()));
        }
      },
      py::arg("file_path"), py::arg("parallel") = true,
      py::arg("num_threads") = 0, py::arg("clustering_config") = py::none(),
      "Process TPX3 file directly to neutron events with clustering");

  // Hits-to-neutrons convenience function
  m.def(
      "cluster_hits_to_neutrons",
      [](const py::object& hits_data,
         py::object clustering_config = py::none()) {
        try {
          // Get clustering configuration
          ClusteringConfig cluster_config;
          if (clustering_config.is_none()) {
            cluster_config = ClusteringConfig::venusDefaults();
          } else if (py::isinstance<ClusteringConfig>(clustering_config)) {
            cluster_config = clustering_config.cast<ClusteringConfig>();
          } else {
            throw TDCConfigError("Invalid clustering configuration type");
          }

          // Convert input hits data
          std::vector<TDCHit> hits;
          if (py::isinstance<std::vector<TDCHit>>(hits_data)) {
            hits = hits_data.cast<std::vector<TDCHit>>();
          } else if (py::isinstance<py::dict>(hits_data)) {
            // Convert from numpy arrays
            auto hits_dict = hits_data.cast<py::dict>();
            if (!hits_dict.contains("x") || !hits_dict.contains("y") ||
                !hits_dict.contains("tof")) {
              throw TDCProcessingError(
                  "Hits dictionary must contain x, y, and tof arrays");
            }

            auto x = hits_dict["x"].cast<py::array_t<uint16_t>>();
            auto y = hits_dict["y"].cast<py::array_t<uint16_t>>();
            auto tof = hits_dict["tof"].cast<py::array_t<uint32_t>>();
            auto tot = hits_dict.contains("tot")
                           ? hits_dict["tot"].cast<py::array_t<uint16_t>>()
                           : py::array_t<uint16_t>();
            auto chip_id =
                hits_dict.contains("chip_id")
                    ? hits_dict["chip_id"].cast<py::array_t<uint8_t>>()
                    : py::array_t<uint8_t>();

            size_t n = x.size();
            hits.reserve(n);

            for (size_t i = 0; i < n; ++i) {
              TDCHit hit;
              hit.x = x.at(i);
              hit.y = y.at(i);
              hit.tof = tof.at(i);
              hit.tot = tot.size() > static_cast<ssize_t>(i) ? tot.at(i) : 100;
              hit.chip_id =
                  chip_id.size() > static_cast<ssize_t>(i) ? chip_id.at(i) : 0;
              hit.timestamp =
                  tof.at(i);  // Use TOF as timestamp if not provided
              hits.push_back(hit);
            }
          } else {
            throw TDCProcessingError(
                "Hits must be vector<TDCHit> or dictionary of arrays");
          }

          // Cluster hits into neutrons
          TDCClusterProcessor cluster_processor(cluster_config);
          auto neutrons = cluster_processor.processHits(hits);

          return neutrons_to_numpy(neutrons);
        } catch (const std::exception& e) {
          throw TDCProcessingError("Failed to cluster hits: " +
                                   std::string(e.what()));
        }
      },
      py::arg("hits"), py::arg("clustering_config") = py::none(),
      "Cluster hits into neutron events");
}

}  // namespace tdcsophiread