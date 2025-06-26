// TDCSophiread Python Bindings
// Exposes high-performance TDC processor to Python via pybind11

#include <pybind11/chrono.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "tdc_detector_config.h"
#include "tdc_hit.h"
#include "tdc_processor.h"

namespace py = pybind11;

// Make vector of TDCHit opaque for efficient transfer
PYBIND11_MAKE_OPAQUE(std::vector<tdcsophiread::TDCHit>);

namespace tdcsophiread {

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

PYBIND11_MODULE(_core, m) {
  m.doc() = "High-performance TDC-only TPX3 data processor";

  // Version information
  m.attr("__version__") = PROJECT_VERSION;

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
      .def("get_super_resolution_factor",
           &DetectorConfig::getSuperResolutionFactor,
           "Get super-resolution factor")
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

  // High-level convenience function for simple usage
  m.def(
      "process_tpx3",
      [](const std::string& file_path, bool parallel = true,
         size_t num_threads = 0) {
        auto config = DetectorConfig::venusDefaults();
        TDCProcessor processor(config);

        std::vector<TDCHit> hits;
        if (parallel) {
          hits = processor.processFileParallel(file_path, num_threads);
        } else {
          hits = processor.processFile(file_path);
        }

        return hits_to_numpy(hits);
      },
      py::arg("file_path"), py::arg("parallel") = true,
      py::arg("num_threads") = 0,
      "Process TPX3 file and return numpy arrays (convenience function)");
}

}  // namespace tdcsophiread