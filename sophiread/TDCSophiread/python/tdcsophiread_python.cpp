// TDCSophiread Python Bindings
// Exposes high-performance TDC processor to Python via pybind11

#include <pybind11/chrono.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

// Legacy clustering headers disabled - moved to legacy/
// #include "tdc_cluster_processor.h"
// #include "tdc_clustering_config.h"
// #include "tdc_graph_clustering.h"

#include "tdc_detector_config.h"
#include "tdc_hit.h"
#include "tdc_neutron.h"
#include "tdc_processor.h"

// New neutron processing architecture
#include "neutron_processing/neutron_config.h"
#include "neutron_processing/neutron_factories.h"
#include "neutron_processing/neutron_processing.h"

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

// Wrapper class for zero-copy numpy access to TDCHit vector
class TDCHitView {
 public:
  std::vector<TDCHit> data;

  // Constructor from existing vector (moves data)
  TDCHitView(std::vector<TDCHit>&& hits) : data(std::move(hits)) {}

  // Constructor from existing vector (copies data)
  TDCHitView(const std::vector<TDCHit>& hits) : data(hits) {}

  size_t size() const { return data.size(); }
};

// Wrapper class for zero-copy numpy access to TDCNeutron vector
class TDCNeutronView {
 public:
  std::vector<TDCNeutron> data;

  // Constructor from existing vector (moves data)
  TDCNeutronView(std::vector<TDCNeutron>&& neutrons)
      : data(std::move(neutrons)) {}

  // Constructor from existing vector (copies data)
  TDCNeutronView(const std::vector<TDCNeutron>& neutrons) : data(neutrons) {}

  // Destructor
  ~TDCNeutronView() {}

  size_t size() const { return data.size(); }
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
                     "Hit timestamp (25ns units)")
      .def_readwrite("cluster_id", &TDCHit::cluster_id,
                     "Cluster ID (-1 = unclustered)");

  // Define numpy dtype for TDCHit to enable zero-copy structured arrays
  PYBIND11_NUMPY_DTYPE(TDCHit, tof, x, y, timestamp, tot, chip_id, cluster_id);

  // TDCHitView for zero-copy numpy access
  py::class_<TDCHitView>(m, "TDCHitView", py::buffer_protocol())
      .def(py::init<const std::vector<TDCHit>&>())
      .def("size", &TDCHitView::size)
      .def_buffer([](TDCHitView& view) -> py::buffer_info {
        return py::buffer_info(
            view.data.data(),                        /* Pointer to buffer */
            sizeof(TDCHit),                          /* Size of one scalar */
            py::format_descriptor<TDCHit>::format(), /* Python struct-style
                                                        format descriptor */
            1,                                       /* Number of dimensions */
            {view.data.size()},                      /* Buffer dimensions */
            {sizeof(TDCHit)} /* Strides (in bytes) for each index */
        );
      });

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

  // Define numpy dtype for TDCNeutron to enable zero-copy structured arrays
  PYBIND11_NUMPY_DTYPE(TDCNeutron, x, y, tof, tot, n_hits, chip_id, reserved);

  // TDCNeutronView for zero-copy numpy access
  py::class_<TDCNeutronView>(m, "TDCNeutronView", py::buffer_protocol())
      .def(py::init<const std::vector<TDCNeutron>&>())
      .def("size", &TDCNeutronView::size)
      .def_buffer([](TDCNeutronView& view) -> py::buffer_info {
        return py::buffer_info(
            view.data.data(),   /* Pointer to buffer */
            sizeof(TDCNeutron), /* Size of one scalar */
            py::format_descriptor<TDCNeutron>::format(), /* Python struct-style
                                                            format descriptor */
            1,                   /* Number of dimensions */
            {view.data.size()},  /* Buffer dimensions */
            {sizeof(TDCNeutron)} /* Strides (in bytes) for each index */
        );
      });

  // Bind vector of TDCNeutron for efficient operations
  py::bind_vector<std::vector<TDCNeutron>>(m, "TDCNeutronVector");

  // NEW NEUTRON PROCESSING ARCHITECTURE (Zero-Copy Interface)

  // NeutronProcessingConfig - main configuration
  py::class_<NeutronProcessingConfig>(m, "NeutronProcessingConfig")
      .def(py::init<>(), "Create default configuration")
      .def_static(
          "venus_defaults", &NeutronProcessingConfig::venusDefaults,
          "Create VENUS detector default neutron processing configuration")
      .def_readwrite("clustering", &NeutronProcessingConfig::clustering,
                     "Hit clustering configuration")
      .def_readwrite("extraction", &NeutronProcessingConfig::extraction,
                     "Neutron extraction configuration")
      .def_readwrite("temporal", &NeutronProcessingConfig::temporal,
                     "Temporal processing configuration")
      .def("validate", &NeutronProcessingConfig::validate,
           "Validate configuration parameters");

  // HitClusteringConfig
  py::class_<HitClusteringConfig>(m, "HitClusteringConfig")
      .def(py::init<>())
      .def_readwrite(
          "algorithm", &HitClusteringConfig::algorithm,
          "Clustering algorithm name ('abs', 'graph', 'dbscan', or 'grid')")
      .def_readwrite("abs", &HitClusteringConfig::abs,
                     "ABS clustering parameters")
      .def_readwrite("graph", &HitClusteringConfig::graph,
                     "Graph clustering parameters")
      .def_readwrite("dbscan", &HitClusteringConfig::dbscan,
                     "DBSCAN clustering parameters")
      .def_readwrite("grid", &HitClusteringConfig::grid,
                     "Grid clustering parameters");

  // ABSConfig
  py::class_<ABSConfig>(m, "ABSConfig")
      .def(py::init<>())
      .def_readwrite("radius", &ABSConfig::radius,
                     "Spatial clustering radius in pixels")
      .def_readwrite("min_cluster_size", &ABSConfig::min_cluster_size,
                     "Minimum hits per cluster")
      .def_readwrite("neutron_correlation_window",
                     &ABSConfig::neutron_correlation_window,
                     "Neutron temporal correlation window in nanoseconds");

  // GraphConfig
  py::class_<GraphConfig>(m, "GraphConfig")
      .def(py::init<>())
      .def_readwrite("radius", &GraphConfig::radius,
                     "Spatial clustering radius in pixels")
      .def_readwrite("min_cluster_size", &GraphConfig::min_cluster_size,
                     "Minimum hits per cluster")
      .def_readwrite("grid_size", &GraphConfig::grid_size,
                     "Spatial grid size for hashing")
      .def_readwrite("enable_spatial_hash", &GraphConfig::enable_spatial_hash,
                     "Enable spatial hash optimization")
      .def_readwrite("parallel_threshold", &GraphConfig::parallel_threshold,
                     "Minimum hits for parallel processing");

  // DBSCANConfig
  py::class_<DBSCANConfig>(m, "DBSCANConfig")
      .def(py::init<>())
      .def_readwrite("epsilon", &DBSCANConfig::epsilon,
                     "Maximum distance between neighbor points")
      .def_readwrite("min_points", &DBSCANConfig::min_points,
                     "Minimum points to form dense region")
      .def_readwrite("neutron_correlation_window",
                     &DBSCANConfig::neutron_correlation_window,
                     "Temporal correlation window in nanoseconds")
      .def_readwrite("grid_size", &DBSCANConfig::grid_size,
                     "Spatial grid size for neighbor search");

  // GridConfig
  py::class_<GridConfig>(m, "GridConfig")
      .def(py::init<>())
      .def_readwrite("grid_cols", &GridConfig::grid_cols,
                     "Number of grid columns")
      .def_readwrite("grid_rows", &GridConfig::grid_rows, "Number of grid rows")
      .def_readwrite("connection_distance", &GridConfig::connection_distance,
                     "Maximum distance to connect hits")
      .def_readwrite("neutron_correlation_window",
                     &GridConfig::neutron_correlation_window,
                     "Temporal correlation window in nanoseconds")
      .def_readwrite("merge_adjacent_cells", &GridConfig::merge_adjacent_cells,
                     "Merge clusters across cell boundaries");

  // NeutronExtractionConfig
  py::class_<NeutronExtractionConfig>(m, "NeutronExtractionConfig")
      .def(py::init<>())
      .def_readwrite("algorithm", &NeutronExtractionConfig::algorithm,
                     "Extraction algorithm name ('simple_centroid')")
      .def_readwrite("super_resolution_factor",
                     &NeutronExtractionConfig::super_resolution_factor,
                     "Sub-pixel resolution scaling factor")
      .def_readwrite("weighted_by_tot",
                     &NeutronExtractionConfig::weighted_by_tot,
                     "Use TOT weighting for centroid calculation")
      .def_readwrite("min_tot_threshold",
                     &NeutronExtractionConfig::min_tot_threshold,
                     "Minimum TOT threshold for hit inclusion");

  // TemporalProcessingConfig
  py::class_<TemporalProcessingConfig>(m, "TemporalProcessingConfig")
      .def(py::init<>())
      .def_readwrite("num_workers", &TemporalProcessingConfig::num_workers,
                     "Number of worker threads (0 = auto-detect)")
      .def_readwrite("min_batch_size",
                     &TemporalProcessingConfig::min_batch_size,
                     "Minimum hits per batch")
      .def_readwrite("max_batch_size",
                     &TemporalProcessingConfig::max_batch_size,
                     "Maximum hits per batch")
      .def_readwrite("overlap_factor",
                     &TemporalProcessingConfig::overlap_factor,
                     "Overlap size multiplier")
      .def_readwrite("enable_deduplication",
                     &TemporalProcessingConfig::enable_deduplication,
                     "Enable neutron deduplication")
      .def_readwrite("deduplication_tolerance",
                     &TemporalProcessingConfig::deduplication_tolerance,
                     "Spatial tolerance for deduplication");

  // ProcessingStatistics
  py::class_<ProcessingStatistics>(m, "ProcessingStatistics")
      .def(py::init<>())
      .def_readonly("total_hits_processed",
                    &ProcessingStatistics::total_hits_processed,
                    "Total hits processed")
      .def_readonly("total_neutrons_produced",
                    &ProcessingStatistics::total_neutrons_produced,
                    "Total neutrons produced")
      .def_readonly("total_processing_time_ms",
                    &ProcessingStatistics::total_processing_time_ms,
                    "Total processing time in milliseconds")
      .def_readonly("hits_per_second", &ProcessingStatistics::hits_per_second,
                    "Processing rate in hits per second")
      .def_readonly("neutron_efficiency",
                    &ProcessingStatistics::neutron_efficiency,
                    "Neutron efficiency (neutrons/hits ratio)");

  // TemporalNeutronProcessor - main zero-copy interface
  py::class_<TemporalNeutronProcessor>(m, "TemporalNeutronProcessor")
      .def(py::init<>(), "Create processor with VENUS defaults")
      .def(py::init<const NeutronProcessingConfig&>(), py::arg("config"),
           "Create processor with custom configuration")

      // Main zero-copy processing interface
      .def(
          "processHits",
          [](TemporalNeutronProcessor& self, const std::vector<TDCHit>& hits,
             size_t start_offset = 0, size_t end_offset = SIZE_MAX) {
            auto neutrons = self.processHits(hits, start_offset, end_offset);
            return TDCNeutronView(std::move(neutrons));
          },
          py::arg("hits"), py::arg("start_offset") = 0,
          py::arg("end_offset") = SIZE_MAX,
          "Process hits using zero-copy temporal batching (high performance)")

      // Configuration management
      .def("configure", &TemporalNeutronProcessor::configure, py::arg("config"),
           "Update processor configuration")
      .def("getConfig", &TemporalNeutronProcessor::getConfig,
           "Get current configuration",
           py::return_value_policy::reference_internal)

      // Performance metrics
      .def("getLastProcessingTimeMs",
           &TemporalNeutronProcessor::getLastProcessingTimeMs,
           "Get processing time for last operation")
      .def("getLastHitsPerSecond",
           &TemporalNeutronProcessor::getLastHitsPerSecond,
           "Get processing rate for last operation")
      .def("getLastNeutronEfficiency",
           &TemporalNeutronProcessor::getLastNeutronEfficiency,
           "Get neutron efficiency from last operation")
      .def("getStatistics", &TemporalNeutronProcessor::getStatistics,
           "Get detailed processing statistics", py::return_value_policy::copy)

      // Algorithm info
      .def("getHitClusteringAlgorithm",
           &TemporalNeutronProcessor::getHitClusteringAlgorithm,
           "Get clustering algorithm name")
      .def("getNeutronExtractionAlgorithm",
           &TemporalNeutronProcessor::getNeutronExtractionAlgorithm,
           "Get extraction algorithm name")
      .def("getNumWorkers", &TemporalNeutronProcessor::getNumWorkers,
           "Get number of worker threads")

      // Utilities
      .def("reset", &TemporalNeutronProcessor::reset, "Reset processor state");

  // High-level convenience function for hits -> neutrons processing
  m.def(
      "process_hits_to_neutrons",
      [](const py::object& hits_data, py::object config_obj = py::none()) {
        try {
          // Get configuration
          NeutronProcessingConfig config;
          if (config_obj.is_none()) {
            config = NeutronProcessingConfig::venusDefaults();
          } else if (py::isinstance<NeutronProcessingConfig>(config_obj)) {
            config = config_obj.cast<NeutronProcessingConfig>();
          } else {
            throw TDCConfigError(
                "Invalid neutron processing configuration type");
          }

          // Convert input hits data (similar to legacy code but simpler)
          std::vector<TDCHit> hits;
          if (py::isinstance<std::vector<TDCHit>>(hits_data)) {
            hits = hits_data.cast<std::vector<TDCHit>>();
          } else if (py::isinstance<TDCHitView>(hits_data)) {
            auto hit_view = hits_data.cast<TDCHitView>();
            hits = hit_view.data;
          } else if (py::isinstance<py::array>(hits_data)) {
            // Handle structured numpy array
            auto arr = hits_data.cast<py::array>();
            if (arr.dtype().kind() == 'V') {  // Structured array
              // Get the buffer info to access raw data
              py::buffer_info buf = arr.request();
              if (buf.itemsize != sizeof(TDCHit)) {
                throw TDCProcessingError(
                    "Numpy array itemsize does not match TDCHit size");
              }

              // Cast buffer data to TDCHit array
              TDCHit* hit_ptr = static_cast<TDCHit*>(buf.ptr);
              size_t n_hits = buf.size;

              // Use direct memory copy instead of loop
              hits.resize(n_hits);
              std::memcpy(hits.data(), hit_ptr, n_hits * sizeof(TDCHit));
            } else {
              throw TDCProcessingError(
                  "Hits must be structured numpy array (use process_tpx3 or "
                  "hits_to_numpy_view)");
            }
          } else {
            throw TDCProcessingError(
                "Hits must be vector<TDCHit>, TDCHitView, or structured numpy "
                "array");
          }

          // Process using zero-copy interface
          TemporalNeutronProcessor processor(config);
          auto neutrons = processor.processHits(hits);
          auto result = TDCNeutronView(std::move(neutrons));
          return result;
        } catch (const std::exception& e) {
          throw TDCProcessingError("Failed to process hits to neutrons: " +
                                   std::string(e.what()));
        }
      },
      py::arg("hits"), py::arg("config") = py::none(),
      "Process hits to neutrons using zero-copy temporal processor");

  // TDCProcessor class - main interface
  py::class_<TDCProcessor>(m, "TDCProcessor")
      .def(py::init<const DetectorConfig&>(), py::arg("config"),
           "Create processor with detector configuration")

      // Chunk-based processing with optional parallelization
      .def("process_file", &TDCProcessor::processFile, py::arg("file_path"),
           py::arg("chunk_size_mb") = 512, py::arg("parallel") = false,
           py::arg("num_threads") = 0,
           "Process TPX3 file with chunk-based memory mapping")

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

  // Zero-copy function to create structured numpy array view
  m.def(
      "hits_to_numpy_view",
      [](const std::vector<TDCHit>& hits) { return TDCHitView(hits); },
      py::arg("hits"),
      "Create zero-copy TDCHitView for structured numpy array access",
      py::return_value_policy::move);

  // Zero-copy function to create structured numpy array view for neutrons
  m.def(
      "neutrons_to_numpy_view",
      [](const std::vector<TDCNeutron>& neutrons) {
        return TDCNeutronView(neutrons);
      },
      py::arg("neutrons"),
      "Create zero-copy TDCNeutronView for structured numpy array access",
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
          hits = processor.processFile(file_path, 512, parallel, num_threads);

          if (callback.is_valid()) {
            callback(1.0, "Processing complete");
          }

          return TDCHitView(std::move(hits));
        } catch (const std::exception& e) {
          throw TDCProcessingError("Failed to process TPX3 file: " +
                                   std::string(e.what()));
        }
      },
      py::arg("file_path"), py::arg("parallel") = true,
      py::arg("num_threads") = 0, py::arg("progress_callback") = py::none(),
      "Process TPX3 file and return numpy arrays (convenience function with "
      "progress support)");

  // Enhanced streaming function for large files (now uses chunk-based
  // processFile)
  m.def(
      "process_tpx3_stream",
      [](const std::string& file_path, size_t chunk_size_mb = 512,
         py::object progress_callback = py::none()) {
        try {
          auto config = DetectorConfig::venusDefaults();
          TDCProcessor processor(config);

          ProgressCallback callback;
          if (!progress_callback.is_none()) {
            callback = ProgressCallback(progress_callback.cast<py::function>());
          }
          if (callback.is_valid()) {
            callback(0.0, "Starting TPX3 streaming...");
          }

          auto hits = processor.processFile(file_path, chunk_size_mb, false, 0);

          if (callback.is_valid()) {
            callback(1.0, "Processing complete");
          }

          return TDCHitView(std::move(hits));
        } catch (const std::exception& e) {
          throw TDCProcessingError("Failed to stream TPX3 file: " +
                                   std::string(e.what()));
        }
      },
      py::arg("file_path"), py::arg("chunk_size_mb") = 512,
      py::arg("progress_callback") = py::none(),
      "Process large TPX3 files with chunk-based memory mapping");
}

}  // namespace tdcsophiread