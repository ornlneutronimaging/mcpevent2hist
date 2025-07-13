// TDCSophiread Python Bindings
// Exposes high-performance TDC processor to Python via pybind11

#include <pybind11/chrono.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <fstream>
#include <stdexcept>

// Legacy clustering headers disabled - moved to legacy/
// #include "tdc_cluster_processor.h"
// #include "tdc_clustering_config.h"
// #include "tdc_graph_clustering.h"

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

  /*
  ========================================================================
  LEGACY CLUSTERING BINDINGS DISABLED - MOVED TO legacy/ FOLDER
  All clustering-related Python bindings are commented out until the new
  neutron processing architecture is implemented.
  ========================================================================

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
      .def_readwrite("temporal_graph", &ClusteringConfig::temporal_graph,
                     "Temporal graph clustering configuration")
      .def("summary", &ClusteringConfig::summary, "Get configuration summary");

  // GraphConfig for graph clustering algorithm
  py::class_<GraphConfig>(m, "GraphConfig")
      .def(py::init<>())
      .def_readwrite("radius", &GraphConfig::radius,
                     "Spatial clustering radius in pixels")
      .def_readwrite("min_cluster_size", &GraphConfig::min_cluster_size,
                     "Minimum hits per cluster")
      .def_readwrite("neutron_correlation_window",
                     &GraphConfig::neutron_correlation_window,
                     "Neutron temporal correlation window in nanoseconds")
      .def_readwrite("grid_size", &GraphConfig::grid_size,
                     "Spatial grid size for hashing")
      .def_readwrite("enable_spatial_hash", &GraphConfig::enable_spatial_hash,
                     "Enable spatial hash optimization")
      .def_readwrite("parallel_threshold", &GraphConfig::parallel_threshold,
                     "Minimum hits for parallel processing")
      .def("validate", &GraphConfig::validate,
           "Validate configuration parameters");

  // BatchStats for temporal batching analysis
  py::class_<BatchStats>(m, "BatchStats")
      .def(py::init<>())
      .def_readonly("mean_hits_per_window", &BatchStats::mean_hits_per_window,
                    "Average hits within correlation window")
      .def_readonly("std_hits_per_window", &BatchStats::std_hits_per_window,
                    "Standard deviation of hits per window")
      .def_readonly("optimal_window_tof", &BatchStats::optimal_window_tof,
                    "Optimal window size in TOF units (25ns)")
      .def_readonly("overlap_size", &BatchStats::overlap_size,
                    "Overlap size in hits (3σ for boundary handling)")
      .def_readonly("num_pulses_analyzed", &BatchStats::num_pulses_analyzed,
                    "Number of complete pulses analyzed")
      .def_readonly("pulse_period_tof", &BatchStats::pulse_period_tof,
                    "TOF period between pulses (25ns units)")
      .def_readonly("total_hits_analyzed", &BatchStats::total_hits_analyzed,
                    "Total hits used for statistics");

  // HitBatch for temporal batch definition
  py::class_<HitBatch>(m, "HitBatch")
      .def(py::init<>())
      .def_readonly("start_index", &HitBatch::start_index,
                    "Batch start index (inclusive)")
      .def_readonly("end_index", &HitBatch::end_index,
                    "Batch end index (exclusive)")
      .def_readonly("overlap_start", &HitBatch::overlap_start,
                    "Overlap region start index")
      .def_readonly("overlap_end", &HitBatch::overlap_end,
                    "Overlap region end index")
      .def_readonly("tof_window_start", &HitBatch::tof_window_start,
                    "TOF range start for this batch")
      .def_readonly("tof_window_end", &HitBatch::tof_window_end,
                    "TOF range end for this batch")
      .def("size", &HitBatch::size, "Get number of hits in this batch")
      .def("isValid", &HitBatch::isValid, "Check if batch is valid");

  // GraphClustering static methods for temporal batching
  py::class_<GraphClustering>(m, "GraphClustering")
      .def_static("analyzeHitDistribution",
                  &GraphClustering::analyzeHitDistribution, py::arg("hits"),
                  py::arg("num_pulses") = 2,
                  py::arg("correlation_window") = 75.0,
                  "Analyze hit distribution for temporal batching")
      .def_static("createStatisticalBatches",
                  &GraphClustering::createStatisticalBatches, py::arg("hits"),
                  py::arg("stats"), "Create temporal batches from hit vector");

  // TemporalGraphClusteringProcessor configuration
  py::class_<TemporalGraphConfig>(m, "TemporalGraphConfig")
      .def(py::init<>())
      .def_readwrite("graph_config", &TemporalGraphConfig::graph_config,
                     "Base graph clustering configuration")
      .def_readwrite("num_workers", &TemporalGraphConfig::num_workers,
                     "Number of worker threads (0 = auto-detect)")
      .def_readwrite("min_batch_size", &TemporalGraphConfig::min_batch_size,
                     "Minimum hits per batch")
      .def_readwrite("max_batch_size", &TemporalGraphConfig::max_batch_size,
                     "Maximum hits per batch")
      .def_readwrite("overlap_factor", &TemporalGraphConfig::overlap_factor,
                     "Overlap size multiplier (default: 3.0 for 3σ)")
      .def_readwrite("enable_memory_pools",
                     &TemporalGraphConfig::enable_memory_pools,
                     "Enable per-worker memory pools")
      .def_readwrite("enable_temporal_aging",
                     &TemporalGraphConfig::enable_temporal_aging,
                     "Enable temporal aging within batches");

  // TemporalGraphClusteringProcessor statistics
  py::class_<TemporalGraphClusteringProcessor::ProcessingStats>(
      m, "TemporalProcessingStats")
      .def(py::init<>())
      .def_readonly("total_hits_processed",
                    &TemporalGraphClusteringProcessor::ProcessingStats::
                        total_hits_processed,
                    "Total hits processed")
      .def_readonly("total_neutrons_produced",
                    &TemporalGraphClusteringProcessor::ProcessingStats::
                        total_neutrons_produced,
                    "Total neutrons produced")
      .def_readonly("num_batches_created",
                    &TemporalGraphClusteringProcessor::ProcessingStats::
                        num_batches_created,
                    "Number of temporal batches created")
      .def_readonly(
          "num_workers_used",
          &TemporalGraphClusteringProcessor::ProcessingStats::num_workers_used,
          "Number of worker threads used")
      .def_readonly(
          "analysis_time_ms",
          &TemporalGraphClusteringProcessor::ProcessingStats::analysis_time_ms,
          "Time for hit distribution analysis")
      .def_readonly(
          "batching_time_ms",
          &TemporalGraphClusteringProcessor::ProcessingStats::batching_time_ms,
          "Time for batch creation")
      .def_readonly("processing_time_ms",
                    &TemporalGraphClusteringProcessor::ProcessingStats::
                        processing_time_ms,
                    "Time for parallel batch processing")
      .def_readonly("aggregation_time_ms",
                    &TemporalGraphClusteringProcessor::ProcessingStats::
                        aggregation_time_ms,
                    "Time for result aggregation")
      .def_readonly(
          "total_time_ms",
          &TemporalGraphClusteringProcessor::ProcessingStats::total_time_ms,
          "Total processing time")
      .def_readonly(
          "hits_per_second",
          &TemporalGraphClusteringProcessor::ProcessingStats::hits_per_second,
          "Processing rate")
      .def_readonly("neutron_efficiency",
                    &TemporalGraphClusteringProcessor::ProcessingStats::
                        neutron_efficiency,
                    "Neutrons per hit ratio");

  // TemporalGraphClusteringProcessor - main interface
  py::class_<TemporalGraphClusteringProcessor>(
      m, "TemporalGraphClusteringProcessor")
      .def(py::init<>(), "Create processor with default configuration")
      .def(py::init<const TemporalGraphConfig&>(), py::arg("config"),
           "Create processor with custom configuration")
      .def(
          "processHits",
          [](TemporalGraphClusteringProcessor& self,
             const std::vector<TDCHit>& hits) {
            auto neutrons = self.processHits(hits);
            return TDCNeutronView(std::move(neutrons));
          },
          py::arg("hits"),
          "Process hits using temporal batching for high performance")
      .def("getStatistics", &TemporalGraphClusteringProcessor::getStatistics,
           "Get processing statistics from last operation",
           py::return_value_policy::reference_internal)
      .def("updateConfig", &TemporalGraphClusteringProcessor::updateConfig,
           py::arg("config"), "Update processor configuration")
      .def("getConfig", &TemporalGraphClusteringProcessor::getConfig,
           "Get current configuration",
           py::return_value_policy::reference_internal)
      .def("reset", &TemporalGraphClusteringProcessor::reset,
           "Reset processor state and statistics");

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
  */

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

  /*
  // LEGACY CLUSTERING CONVENIENCE FUNCTIONS DISABLED
  // These will be re-implemented with the new neutron processing architecture

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
          hits = processor.processFile(file_path, 512, parallel, num_threads);

          // Cluster hits into neutrons - now memory-optimized internally
          TDCClusterProcessor cluster_processor(cluster_config);
          auto neutrons = cluster_processor.processHits(hits);

          return TDCNeutronView(std::move(neutrons));
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
          } else if (py::isinstance<TDCHitView>(hits_data)) {
            // Handle TDCHitView (zero-copy)
            auto hit_view = hits_data.cast<TDCHitView>();
            hits = hit_view.data;  // Use the vector directly
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

              // Copy data from numpy array to vector
              hits.reserve(n_hits);
              for (size_t i = 0; i < n_hits; ++i) {
                hits.push_back(hit_ptr[i]);
              }
            } else {
              throw TDCProcessingError(
                  "Numpy array must be structured array with TDCHit dtype");
            }
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
                "Hits must be vector<TDCHit>, TDCHitView, structured numpy "
                "array, or dictionary of arrays");
          }

          // Cluster hits into neutrons
          TDCClusterProcessor cluster_processor(cluster_config);
          auto neutrons = cluster_processor.processHits(hits);

          return TDCNeutronView(std::move(neutrons));
        } catch (const std::exception& e) {
          throw TDCProcessingError("Failed to cluster hits: " +
                                   std::string(e.what()));
        }
      },
      py::arg("hits"), py::arg("clustering_config") = py::none(),
      "Cluster hits into neutron events");
  */
}

}  // namespace tdcsophiread