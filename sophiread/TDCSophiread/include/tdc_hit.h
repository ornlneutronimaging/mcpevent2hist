// TDCSophiread Hit Structure and Conversion
// TDC-only hit data structure and packet conversion functions
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <cstdint>

#include "tdc_detector_config.h"
#include "tdc_packet.h"

namespace tdcsophiread {

/**
 * @brief TDC-only hit data structure (cache-optimized layout)
 *
 * Simplified hit representation containing only essential data for TDC-based
 * processing. This structure is the output of TPX3 packet processing and
 * input to higher-level analysis.
 *
 * Fields are ordered by access frequency for optimal cache performance:
 * - TOF (most accessed) first for cache efficiency
 * - X,Y coordinates adjacent for spatial locality
 * - Less frequently accessed fields at end
 *
 * All timing values are in 25ns units (LSB = 25 nanoseconds)
 */
struct TDCHit {
  uint32_t tof;  ///< Time-of-flight (25ns units, after TDC correction) - MOST
                 ///< ACCESSED
  uint16_t x;    ///< Global X coordinate (after chip mapping) - HIGH ACCESS
  uint16_t y;    ///< Global Y coordinate (after chip mapping) - HIGH ACCESS
  uint32_t timestamp;  ///< Hit timestamp (25ns units, after rollover
                       ///< correction) - LOW ACCESS
  uint16_t tot;     ///< Time-over-threshold (10-bit raw value) - MEDIUM ACCESS
  uint8_t chip_id;  ///< Chip identifier (0-3) - LOW ACCESS
  int32_t cluster_id;  ///< Cluster label assigned by clustering algorithm (-1 =
                       ///< unclustered)

  /**
   * @brief Default constructor
   */
  TDCHit()
      : tof(0), x(0), y(0), timestamp(0), tot(0), chip_id(0), cluster_id(-1) {}

  /**
   * @brief Constructor with all fields (ordered by new layout)
   */
  TDCHit(uint32_t tof, uint16_t x, uint16_t y, uint32_t timestamp, uint16_t tot,
         uint8_t chip_id)
      : tof(tof),
        x(x),
        y(y),
        timestamp(timestamp),
        tot(tot),
        chip_id(chip_id),
        cluster_id(-1) {}
};

/**
 * @brief Convert TPX3 hit packet to TDCHit with TDC correction
 *
 * Performs the complete packet-to-hit conversion including:
 * 1. Extract pixel coordinates and timing from packet
 * 2. Apply rollover detection and correction
 * 3. Calculate time-of-flight with missing TDC correction
 * 4. Map local chip coordinates to global detector coordinates
 *
 * Based on Python reference implementation from Vlad's notebook.
 *
 * @param packet TPX3 hit packet to convert
 * @param chip_id Current chip ID (from TPX3 header)
 * @param tdc_timestamp Current TDC timestamp for this chip (25ns units)
 * @param config Detector configuration for coordinate mapping and TDC frequency
 * @return TDCHit Converted hit with global coordinates and corrected TOF
 *
 * @throws std::invalid_argument if packet is not a hit packet
 */
TDCHit convertPacketToHit(const TPX3Packet& packet, uint8_t chip_id,
                          uint32_t tdc_timestamp, const DetectorConfig& config);

/**
 * @brief Convert TPX3 hit packet to TDCHit with optional TDC correction
 *
 * Same as above but allows control over TDC correction application.
 *
 * @param packet TPX3 hit packet to convert
 * @param chip_id Current chip ID (from TPX3 header)
 * @param tdc_timestamp Current TDC timestamp for this chip (25ns units)
 * @param config Detector configuration for coordinate mapping and TDC frequency
 * @param apply_tdc_correction Whether to apply missing TDC correction
 * @return TDCHit Converted hit with global coordinates and optionally corrected
 * TOF
 */
TDCHit convertPacketToHit(const TPX3Packet& packet, uint8_t chip_id,
                          uint32_t tdc_timestamp, const DetectorConfig& config,
                          bool apply_tdc_correction);

/**
 * @brief Apply missing TDC correction to time-of-flight (optimized)
 *
 * Implements the critical algorithm from Python reference:
 * if TOF * 25ns > 1/TDC_frequency, subtract one TDC period
 *
 * This corrects for missing TDC pulses in the data stream.
 * Uses pre-calculated values from DetectorConfig to eliminate billions of
 * FLOPs.
 *
 * @param tof_uncorrected Raw time-of-flight in 25ns units
 * @param config Detector configuration with pre-calculated TDC values
 * @return uint32_t Corrected time-of-flight in 25ns units
 */
uint32_t applyMissingTDCCorrection(uint32_t tof_uncorrected,
                                   const DetectorConfig& config);

/**
 * @brief Detect and correct timestamp rollover
 *
 * Implements rollover detection from Python reference:
 * if hit_timestamp + 0x400000 < tdc_timestamp, extend with 0x40000000
 *
 * @param hit_timestamp 30-bit hit timestamp from packet
 * @param tdc_timestamp Current TDC timestamp
 * @return uint32_t Extended timestamp if rollover detected, otherwise original
 */
uint32_t correctTimestampRollover(uint32_t hit_timestamp,
                                  uint32_t tdc_timestamp);

}  // namespace tdcsophiread