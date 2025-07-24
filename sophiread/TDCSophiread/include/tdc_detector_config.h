// TDCSophiread DetectorConfig Header
// Centralized detector configuration with JSON loading and VENUS defaults

#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace tdcsophiread {

/**
 * @brief 2D affine transformation matrix for chip coordinate mapping
 *
 * Represents a 2x3 affine transformation matrix:
 * [a  b  tx]
 * [c  d  ty]
 *
 * Transforms local chip coordinates to global detector coordinates:
 * global_x = a * local_x + b * local_y + tx
 * global_y = c * local_x + d * local_y + ty
 *
 * Common transformations:
 * - Identity: [[1,0,0], [0,1,0]]
 * - Translation: [[1,0,tx], [0,1,ty]]
 * - X-flip: [[-1,0,255], [0,1,0]]
 * - Y-flip: [[1,0,0], [0,-1,255]]
 * - 180° rotation: [[-1,0,255], [0,-1,255]]
 */
struct ChipTransform {
  double matrix[2][3];  // 2x3 affine transformation matrix

  /**
   * @brief Default constructor - creates identity transform
   */
  ChipTransform() {
    matrix[0][0] = 1.0;
    matrix[0][1] = 0.0;
    matrix[0][2] = 0.0;  // [1, 0, 0]
    matrix[1][0] = 0.0;
    matrix[1][1] = 1.0;
    matrix[1][2] = 0.0;  // [0, 1, 0]
  }

  /**
   * @brief Construct transform from matrix elements
   */
  ChipTransform(double a, double b, double tx, double c, double d, double ty) {
    matrix[0][0] = a;
    matrix[0][1] = b;
    matrix[0][2] = tx;
    matrix[1][0] = c;
    matrix[1][1] = d;
    matrix[1][2] = ty;
  }

  /**
   * @brief Apply transformation to local coordinates
   * @param local_x Local X coordinate within chip
   * @param local_y Local Y coordinate within chip
   * @return Pair of (global_x, global_y) coordinates
   */
  std::pair<int, int> apply(uint16_t local_x, uint16_t local_y) const {
    int global_x = static_cast<int>(matrix[0][0] * local_x +
                                    matrix[0][1] * local_y + matrix[0][2]);
    int global_y = static_cast<int>(matrix[1][0] * local_x +
                                    matrix[1][1] * local_y + matrix[1][2]);
    return {global_x, global_y};
  }
};

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
   * @brief Get pre-calculated TDC period in seconds
   * @return TDC period (1.0 / TDC frequency) - eliminates division in hot path
   */
  double getTdcPeriodSeconds() const { return m_TdcPeriodSeconds; }

  /**
   * @brief Get pre-calculated TDC correction value in 25ns units
   * @return TDC correction value for missing TDC algorithm - eliminates FP math
   * in hot path
   */
  uint32_t getTdcCorrection25ns() const { return m_TdcCorrection25ns; }

  /**
   * @brief Check if missing TDC correction is enabled
   * @return true if correction is enabled (default: true)
   */
  bool isMissingTdcCorrectionEnabled() const {
    return m_EnableMissingTdcCorrection;
  }

  // ==================== CHIP PARAMETERS ====================

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

  /**
   * @brief Get transformation matrix for a specific chip
   * @param chip_id Chip identifier
   * @return ChipTransform for the specified chip
   * @throws std::invalid_argument if chip_id is invalid
   */
  const ChipTransform& getChipTransform(uint16_t chip_id) const;

  /**
   * @brief Set transformation matrix for a specific chip
   * @param chip_id Chip identifier
   * @param transform ChipTransform to set
   * @throws std::invalid_argument if chip_id is invalid
   */
  void setChipTransform(uint16_t chip_id, const ChipTransform& transform);

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

  /**
   * @brief Update pre-calculated TDC values based on current frequency
   */
  void updateTdcCalculations();

  // ==================== MEMBER VARIABLES ====================

  // Timing parameters
  double m_TdcFrequency = 60.0;  // Hz (SNS default)
  bool m_EnableMissingTdcCorrection = true;

  // Pre-calculated TDC values (computed from m_TdcFrequency)
  double m_TdcPeriodSeconds = 1.0 / 60.0;  // 1.0 / m_TdcFrequency
  uint32_t m_TdcCorrection25ns =
      666667;  // (m_TdcPeriodSeconds * 1e9 / 25 + 0.5)

  // Chip parameters
  uint16_t m_ChipSizeX = 256;  // pixels per chip
  uint16_t m_ChipSizeY = 256;

  // Coordinate transformation parameters
  std::vector<ChipTransform>
      m_ChipTransforms;  // Transformation matrix per chip
};

}  // namespace tdcsophiread