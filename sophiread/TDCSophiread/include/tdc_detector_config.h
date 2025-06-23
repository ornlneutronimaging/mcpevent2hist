// TDCSophiread DetectorConfig Header
// Centralized detector configuration with JSON loading and VENUS defaults

#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

namespace tdcsophiread {

/**
 * @brief Detector configuration class for TDC-only TPX3 processing
 *
 * This class centralizes all detector-specific parameters and provides:
 * - Factory methods for common detector configurations (VENUS, ESS, etc.)
 * - JSON file and object loading
 * - Parameter validation
 * - Coordinate mapping functionality
 *
 * Design Goals:
 * - Replace hardcoded constants scattered throughout the codebase
 * - Support multiple facilities and detector geometries
 * - Provide validation to prevent invalid parameter combinations
 * - Enable easy testing with configurable parameters
 */
class DetectorConfig {
 public:
  // ==================== FACTORY METHODS ====================

  /**
   * @brief Create VENUS TPX3 detector defaults
   * @return DetectorConfig configured for VENUS instrument at SNS
   */
  static DetectorConfig venusDefaults();

  /**
   * @brief Load configuration from JSON file
   * @param config_path Path to JSON configuration file
   * @return DetectorConfig loaded from file
   * @throws std::runtime_error if file cannot be read or parsed
   * @throws std::invalid_argument if configuration is invalid
   */
  static DetectorConfig fromFile(const std::string& config_path);

  /**
   * @brief Load configuration from JSON object
   * @param config JSON configuration object
   * @return DetectorConfig loaded from JSON
   * @throws std::invalid_argument if configuration is invalid
   */
  static DetectorConfig fromJson(const nlohmann::json& config);

  // ==================== TIMING PARAMETERS ====================

  /**
   * @brief Get TDC frequency in Hz
   * @return TDC frequency (default: 60.0 Hz for SNS)
   */
  double getTdcFrequency() const { return m_TdcFrequency; }

  /**
   * @brief Check if missing TDC correction is enabled
   * @return true if correction is enabled (default: true)
   */
  bool isMissingTdcCorrectionEnabled() const {
    return m_EnableMissingTdcCorrection;
  }

  // ==================== CHIP LAYOUT PARAMETERS ====================

  /**
   * @brief Get gap between chips in pixels
   * @return Chip gap in pixels (default: 2 for VENUS)
   */
  uint16_t getChipGapPixels() const { return m_ChipGapPixels; }

  /**
   * @brief Get number of chips per row
   * @return Chips per row (default: 2 for 2x2 layout)
   */
  uint16_t getChipsPerRow() const { return m_ChipsPerRow; }

  /**
   * @brief Get number of chips per column
   * @return Chips per column (default: 2 for 2x2 layout)
   */
  uint16_t getChipsPerCol() const { return m_ChipsPerCol; }

  /**
   * @brief Get chip size in X direction
   * @return Chip width in pixels (default: 256)
   */
  uint16_t getChipSizeX() const { return m_ChipSizeX; }

  /**
   * @brief Get chip size in Y direction
   * @return Chip height in pixels (default: 256)
   */
  uint16_t getChipSizeY() const { return m_ChipSizeY; }

  // ==================== SUPER-RESOLUTION PARAMETERS ====================

  /**
   * @brief Get super-resolution factor
   * @return Super-resolution factor (default: 8 for 8x8 sub-pixels)
   */
  uint8_t getSuperResolutionFactor() const { return m_SuperResolutionFactor; }

  // ==================== COORDINATE MAPPING ====================

  /**
   * @brief Map local chip coordinates to global detector coordinates
   * @param chip_id Chip identifier (0-3 for 2x2 layout)
   * @param local_x Local X coordinate within chip
   * @param local_y Local Y coordinate within chip
   * @return Pair of (global_x, global_y) coordinates
   * @throws std::invalid_argument if chip_id is invalid
   */
  std::pair<int, int> mapChipToGlobal(uint16_t chip_id, uint16_t local_x,
                                      uint16_t local_y) const;

 private:
  // ==================== PRIVATE CONSTRUCTOR ====================

  /**
   * @brief Private constructor - use factory methods instead
   */
  DetectorConfig() = default;

  /**
   * @brief Validate configuration parameters
   * @throws std::invalid_argument if any parameter is invalid
   */
  void validateConfig() const;

  // ==================== MEMBER VARIABLES ====================

  // Timing parameters
  double m_TdcFrequency = 60.0;  // Hz (SNS default)
  bool m_EnableMissingTdcCorrection = true;

  // Chip layout parameters
  uint16_t m_ChipGapPixels = 2;  // VENUS TPX3 default
  uint16_t m_ChipsPerRow = 2;
  uint16_t m_ChipsPerCol = 2;
  uint16_t m_ChipSizeX = 256;  // pixels per chip
  uint16_t m_ChipSizeY = 256;

  // Super-resolution parameters
  uint8_t m_SuperResolutionFactor =
      4;  // 4x4 sub-pixels per pixel (VENUS default)
};

}  // namespace tdcsophiread