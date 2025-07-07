// TDCSophiread DetectorConfig Implementation
// Centralized detector configuration with JSON loading and VENUS defaults

#include "tdc_detector_config.h"

#include <cassert>
#include <fstream>
#include <sstream>

namespace tdcsophiread {

// ==================== FACTORY METHODS ====================

DetectorConfig DetectorConfig::venusDefaults() {
  DetectorConfig config;

  // VENUS TPX3 defaults from analysis document
  config.m_TdcFrequency = 60.0;  // Hz (SNS default)
  config.m_EnableMissingTdcCorrection = true;

  config.m_ChipSizeX = 256;
  config.m_ChipSizeY = 256;

  // Initialize transformation matrices for VENUS 2x2 layout with 2-pixel gaps
  // Updated from 5-pixel gaps to match current VENUS detector configuration
  config.m_ChipTransforms.resize(4);

  // Chip 0: x += 258, y unchanged -> [[1, 0, 258], [0, 1, 0]]
  config.m_ChipTransforms[0] = ChipTransform(1.0, 0.0, 258.0, 0.0, 1.0, 0.0);

  // Chip 1: x = 255 - x + 258, y = 255 - y + 258 -> [[-1, 0, 513], [0, -1,
  // 513]]
  config.m_ChipTransforms[1] =
      ChipTransform(-1.0, 0.0, 513.0, 0.0, -1.0, 513.0);

  // Chip 2: x = 255 - x, y = 255 - y + 258 -> [[-1, 0, 255], [0, -1, 513]]
  config.m_ChipTransforms[2] =
      ChipTransform(-1.0, 0.0, 255.0, 0.0, -1.0, 513.0);

  // Chip 3: x unchanged, y unchanged -> [[1, 0, 0], [0, 1, 0]]
  config.m_ChipTransforms[3] = ChipTransform(1.0, 0.0, 0.0, 0.0, 1.0, 0.0);

  config.updateTdcCalculations();
  config.validateConfig();
  return config;
}

DetectorConfig DetectorConfig::fromFile(const std::string& config_path) {
  // Open and read JSON file
  std::ifstream file(config_path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open configuration file: " + config_path);
  }

  nlohmann::json json_config;
  try {
    file >> json_config;
  } catch (const nlohmann::json::parse_error& e) {
    throw std::runtime_error("JSON parse error in " + config_path + ": " +
                             e.what());
  }

  return fromJson(json_config);
}

DetectorConfig DetectorConfig::fromJson(const nlohmann::json& config) {
  DetectorConfig detector_config;

  // Check if top-level "detector" key exists
  if (!config.contains("detector")) {
    throw std::invalid_argument("Configuration must contain 'detector' key");
  }

  const auto& detector = config["detector"];

  // Load timing parameters
  if (detector.contains("timing")) {
    const auto& timing = detector["timing"];

    if (timing.contains("tdc_frequency_hz")) {
      detector_config.m_TdcFrequency = timing["tdc_frequency_hz"];
      detector_config.updateTdcCalculations();
    }

    if (timing.contains("enable_missing_tdc_correction")) {
      detector_config.m_EnableMissingTdcCorrection =
          timing["enable_missing_tdc_correction"];
    }
  }

  // Load chip parameters
  if (detector.contains("chip_layout")) {
    const auto& layout = detector["chip_layout"];

    if (layout.contains("chip_size_x")) {
      detector_config.m_ChipSizeX = layout["chip_size_x"];
    }

    if (layout.contains("chip_size_y")) {
      detector_config.m_ChipSizeY = layout["chip_size_y"];
    }
  }

  // Load chip transformation matrices
  if (detector.contains("chip_transformations")) {
    const auto& transformations = detector["chip_transformations"];

    if (transformations.is_array()) {
      for (const auto& transform_config : transformations) {
        if (transform_config.contains("chip_id") &&
            transform_config.contains("matrix")) {
          uint16_t chip_id = transform_config["chip_id"];
          const auto& matrix = transform_config["matrix"];

          // Validate matrix format: [[a, b, tx], [c, d, ty]]
          if (matrix.is_array() && matrix.size() == 2) {
            const auto& row1 = matrix[0];
            const auto& row2 = matrix[1];

            if (row1.is_array() && row1.size() == 3 && row2.is_array() &&
                row2.size() == 3) {
              ChipTransform transform(row1[0], row1[1], row1[2],  // a, b, tx
                                      row2[0], row2[1], row2[2]   // c, d, ty
              );
              detector_config.setChipTransform(chip_id, transform);
            }
          }
        }
      }
    }
  } else {
    // If no transformations specified, use VENUS defaults (2-pixel gaps)
    // This ensures backward compatibility with existing JSON files
    detector_config.m_ChipTransforms.resize(4);
    detector_config.m_ChipTransforms[0] =
        ChipTransform(1.0, 0.0, 258.0, 0.0, 1.0, 0.0);
    detector_config.m_ChipTransforms[1] =
        ChipTransform(-1.0, 0.0, 513.0, 0.0, -1.0, 513.0);
    detector_config.m_ChipTransforms[2] =
        ChipTransform(-1.0, 0.0, 255.0, 0.0, -1.0, 513.0);
    detector_config.m_ChipTransforms[3] =
        ChipTransform(1.0, 0.0, 0.0, 0.0, 1.0, 0.0);
  }

  detector_config.updateTdcCalculations();
  detector_config.validateConfig();
  return detector_config;
}

// ==================== COORDINATE MAPPING ====================

std::pair<int, int> DetectorConfig::mapChipToGlobal(uint16_t chip_id,
                                                    uint16_t local_x,
                                                    uint16_t local_y) const {
  // Debug-only validation (compiled out in release builds with -DNDEBUG)
  assert(chip_id < m_ChipTransforms.size() &&
         "Invalid chip_id exceeds available transformations");
  assert(local_x < m_ChipSizeX && local_y < m_ChipSizeY &&
         "Local coordinates exceed chip boundaries");

  // Apply transformation matrix for the specified chip
  return m_ChipTransforms[chip_id].apply(local_x, local_y);
}

const ChipTransform& DetectorConfig::getChipTransform(uint16_t chip_id) const {
  if (chip_id >= m_ChipTransforms.size()) {
    throw std::invalid_argument(
        "Invalid chip_id: " + std::to_string(chip_id) +
        " (max: " + std::to_string(m_ChipTransforms.size() - 1) + ")");
  }
  return m_ChipTransforms[chip_id];
}

void DetectorConfig::setChipTransform(uint16_t chip_id,
                                      const ChipTransform& transform) {
  if (chip_id >= m_ChipTransforms.size()) {
    // Resize if necessary
    m_ChipTransforms.resize(chip_id + 1);
  }
  m_ChipTransforms[chip_id] = transform;
}

// ==================== PRIVATE METHODS ====================

void DetectorConfig::updateTdcCalculations() {
  m_TdcPeriodSeconds = 1.0 / m_TdcFrequency;
  m_TdcCorrection25ns =
      static_cast<uint32_t>(m_TdcPeriodSeconds * 1e9 / 25 + 0.5);
}

void DetectorConfig::validateConfig() const {
  // Validate timing parameters
  if (m_TdcFrequency <= 0.0) {
    throw std::invalid_argument("TDC frequency must be positive, got: " +
                                std::to_string(m_TdcFrequency));
  }

  // Validate chip parameters
  if (m_ChipSizeX == 0) {
    throw std::invalid_argument("Chip size X must be positive, got: " +
                                std::to_string(m_ChipSizeX));
  }

  if (m_ChipSizeY == 0) {
    throw std::invalid_argument("Chip size Y must be positive, got: " +
                                std::to_string(m_ChipSizeY));
  }
}

}  // namespace tdcsophiread