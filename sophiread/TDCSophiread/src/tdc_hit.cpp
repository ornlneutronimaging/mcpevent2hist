// TDCSophiread Hit Conversion Implementation
// SPDX-License-Identifier: GPL-3.0+

#include "tdc_hit.h"

#include <stdexcept>

namespace tdcsophiread {

TDCHit convertPacketToHit(const TPX3Packet& packet, uint8_t chip_id,
                          uint32_t tdc_timestamp,
                          const DetectorConfig& config) {
  return convertPacketToHit(packet, chip_id, tdc_timestamp, config, true);
}

TDCHit convertPacketToHit(const TPX3Packet& packet, uint8_t chip_id,
                          uint32_t tdc_timestamp, const DetectorConfig& config,
                          bool apply_tdc_correction) {
  // Validate input
  if (!packet.isHit()) {
    throw std::invalid_argument("Packet is not a hit packet");
  }

  // Extract pixel coordinates (local to chip)
  auto [local_x, local_y] = packet.getPixelCoordinates();

  // Extract timing information
  uint32_t hit_timestamp = packet.getTimestamp25ns();
  uint16_t tot = packet.getToT();

  // Apply rollover correction to timestamp
  uint32_t corrected_timestamp =
      correctTimestampRollover(hit_timestamp, tdc_timestamp);

  // Calculate time-of-flight (only if timestamp >= TDC timestamp)
  uint32_t tof = 0;
  if (corrected_timestamp >= tdc_timestamp) {
    uint32_t raw_tof = corrected_timestamp - tdc_timestamp;

    // Apply missing TDC correction only if requested
    if (apply_tdc_correction) {
      tof = applyMissingTDCCorrection(raw_tof, config);
    } else {
      tof = raw_tof;
    }
  }

  // Map local chip coordinates to global detector coordinates
  auto [global_x, global_y] = config.mapChipToGlobal(chip_id, local_x, local_y);

  // Create and return TDCHit (using new constructor field order)
  return TDCHit(tof, global_x, global_y, corrected_timestamp, tot, chip_id);
}

uint32_t applyMissingTDCCorrection(uint32_t tof_uncorrected,
                                   const DetectorConfig& config) {
  // Convert TOF to seconds: TOF * 25ns
  double tof_seconds = tof_uncorrected * 25e-9;

  // Use pre-calculated TDC period (eliminates division in hot path)
  double tdc_period_seconds = config.getTdcPeriodSeconds();

  // Check if correction is needed (Python: if TOF*25/1e9 > 1/TDC_frequency)
  if (tof_seconds > tdc_period_seconds) {
    // Use pre-calculated correction value (eliminates FP math in hot path)
    uint32_t correction_25ns = config.getTdcCorrection25ns();
    return tof_uncorrected - correction_25ns;
  }

  return tof_uncorrected;
}

uint32_t correctTimestampRollover(uint32_t hit_timestamp,
                                  uint32_t tdc_timestamp) {
  // Python rollover detection: if Timestamp25ns + 0x400000 < TDC_Timestamp25ns
  if ((hit_timestamp + 0x400000) < tdc_timestamp) {
    // Extend timestamp with rollover bit
    return hit_timestamp | 0x40000000;
  }

  return hit_timestamp;
}

}  // namespace tdcsophiread