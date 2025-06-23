// TDCSophiread DetectorConfig Implementation
// Centralized detector configuration with JSON loading and VENUS defaults

#include "tdc_detector_config.h"

#include <fstream>
#include <sstream>

namespace tdcsophiread {

// ==================== FACTORY METHODS ====================

DetectorConfig DetectorConfig::venusDefaults() {
  DetectorConfig config;

  // VENUS TPX3 defaults from analysis document
  config.m_TdcFrequency = 60.0;  // Hz (SNS default)
  config.m_EnableMissingTdcCorrection = true;

  config.m_ChipGapPixels = 2;  // VENUS TPX3 default
  config.m_ChipsPerRow = 2;
  config.m_ChipsPerCol = 2;
  config.m_ChipSizeX = 256;
  config.m_ChipSizeY = 256;

  config.m_SuperResolutionFactor =
      4;  // 4x4 sub-pixels per pixel (VENUS default)

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
    }

    if (timing.contains("enable_missing_tdc_correction")) {
      detector_config.m_EnableMissingTdcCorrection =
          timing["enable_missing_tdc_correction"];
    }
  }

  // Load chip layout parameters
  if (detector.contains("chip_layout")) {
    const auto& layout = detector["chip_layout"];

    if (layout.contains("chip_gap_pixels")) {
      detector_config.m_ChipGapPixels = layout["chip_gap_pixels"];
    }

    if (layout.contains("chips_per_row")) {
      detector_config.m_ChipsPerRow = layout["chips_per_row"];
    }

    if (layout.contains("chips_per_col")) {
      detector_config.m_ChipsPerCol = layout["chips_per_col"];
    }

    if (layout.contains("chip_size_x")) {
      detector_config.m_ChipSizeX = layout["chip_size_x"];
    }

    if (layout.contains("chip_size_y")) {
      detector_config.m_ChipSizeY = layout["chip_size_y"];
    }
  }

  // Load super-resolution parameters
  if (detector.contains("super_resolution")) {
    const auto& super_res = detector["super_resolution"];

    if (super_res.contains("factor")) {
      detector_config.m_SuperResolutionFactor = super_res["factor"];
    }
  }

  detector_config.validateConfig();
  return detector_config;
}

// ==================== COORDINATE MAPPING ====================

std::pair<int, int> DetectorConfig::mapChipToGlobal(uint16_t chip_id,
                                                    uint16_t local_x,
                                                    uint16_t local_y) const {
  // Validate chip_id
  uint16_t total_chips = m_ChipsPerRow * m_ChipsPerCol;
  if (chip_id >= total_chips) {
    throw std::invalid_argument("Invalid chip_id: " + std::to_string(chip_id) +
                                " (max: " + std::to_string(total_chips - 1) +
                                ")");
  }

  // Validate local coordinates
  if (local_x >= m_ChipSizeX || local_y >= m_ChipSizeY) {
    throw std::invalid_argument(
        "Local coordinates out of bounds: (" + std::to_string(local_x) + ", " +
        std::to_string(local_y) + ") max: (" + std::to_string(m_ChipSizeX - 1) +
        ", " + std::to_string(m_ChipSizeY - 1) + ")");
  }

  // Calculate chip row and column from chip_id
  uint16_t chip_row = chip_id / m_ChipsPerRow;
  uint16_t chip_col = chip_id % m_ChipsPerRow;

  // Calculate global coordinates
  int global_x = chip_col * (m_ChipSizeX + m_ChipGapPixels) + local_x;
  int global_y = chip_row * (m_ChipSizeY + m_ChipGapPixels) + local_y;

  return {global_x, global_y};
}

// ==================== PRIVATE METHODS ====================

void DetectorConfig::validateConfig() const {
  // Validate timing parameters
  if (m_TdcFrequency <= 0.0) {
    throw std::invalid_argument("TDC frequency must be positive, got: " +
                                std::to_string(m_TdcFrequency));
  }

  // Validate chip layout parameters
  if (m_ChipsPerRow == 0) {
    throw std::invalid_argument("Chips per row must be positive, got: " +
                                std::to_string(m_ChipsPerRow));
  }

  if (m_ChipsPerCol == 0) {
    throw std::invalid_argument("Chips per column must be positive, got: " +
                                std::to_string(m_ChipsPerCol));
  }

  if (m_ChipSizeX == 0) {
    throw std::invalid_argument("Chip size X must be positive, got: " +
                                std::to_string(m_ChipSizeX));
  }

  if (m_ChipSizeY == 0) {
    throw std::invalid_argument("Chip size Y must be positive, got: " +
                                std::to_string(m_ChipSizeY));
  }

  if (m_SuperResolutionFactor == 0) {
    throw std::invalid_argument(
        "Super resolution factor must be positive, got: " +
        std::to_string(m_SuperResolutionFactor));
  }
}

}  // namespace tdcsophiread