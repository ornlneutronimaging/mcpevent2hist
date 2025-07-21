// TDCSophiread Hit Processing Tests
// TDD approach: Tests for TPX3 packet to TDCHit conversion with TDC correction

#include <gtest/gtest.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "tdc_detector_config.h"
#include "tdc_hit.h"
#include "tdc_packet.h"

namespace tdcsophiread {

// Test class for hit processing
class TDCHitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Use VENUS defaults for testing
    config = std::make_unique<DetectorConfig>(DetectorConfig::venusDefaults());
  }

  // Helper to create TDC packet
  uint64_t createTDCPacket(uint32_t timestamp_30bit) {
    uint64_t packet = 0x6F00000000000000ULL;  // TDC packet ID
    packet |= (static_cast<uint64_t>(timestamp_30bit & 0x3FFFFFFF) << 12);
    return packet;
  }

  // Helper to create hit packet
  uint64_t createHitPacket(uint16_t pixel_addr, uint16_t toa,
                           uint16_t spidr_time) {
    uint64_t packet = 0xB000000000000000ULL;  // Hit packet ID
    packet |= (static_cast<uint64_t>(pixel_addr & 0xFFFF) << 44);
    packet |= (static_cast<uint64_t>(toa & 0x3FFF) << 30);
    packet |= (spidr_time & 0xFFFF);
    return packet;
  }

  std::unique_ptr<DetectorConfig> config;
};

// Test 1: TDCHit should store basic hit information
TEST_F(TDCHitTest, StoresBasicHitInformation) {
  // GREEN PHASE: Testing actual implementation

  // Test creating a basic TDCHit structure
  TDCHit hit;
  hit.x = 100;
  hit.y = 150;
  hit.tof = 1000000;  // 25ms in 25ns units
  hit.tot = 50;
  hit.chip_id = 2;
  hit.timestamp = 0x12345678;

  EXPECT_EQ(hit.x, 100);
  EXPECT_EQ(hit.y, 150);
  EXPECT_EQ(hit.tof, 1000000);
  EXPECT_EQ(hit.tot, 50);
  EXPECT_EQ(hit.chip_id, 2);
  EXPECT_EQ(hit.timestamp, 0x12345678);

  // Test constructor (new parameter order: tof, x, y, timestamp, tot, chip_id)
  TDCHit hit2(2000000, 200, 250, 0x87654321, 75, 3);
  EXPECT_EQ(hit2.x, 200);
  EXPECT_EQ(hit2.y, 250);
  EXPECT_EQ(hit2.tof, 2000000);
  EXPECT_EQ(hit2.tot, 75);
  EXPECT_EQ(hit2.chip_id, 3);
  EXPECT_EQ(hit2.timestamp, 0x87654321);
}

// Test 2: Should convert hit packet to TDCHit with coordinate mapping
TEST_F(TDCHitTest, ConvertsHitPacketWithCoordinateMapping) {
  // RED PHASE: This will fail initially

  // Test converting a hit packet from chip 1 to global coordinates
  // Create a realistic pixel address: dcol=4, spix=4, pix=0
  // pixel_addr format: dcol=(addr&0xFE00)>>8, spix=(addr&0x1F8)>>1,
  // pix=addr&0x7 So: addr = (dcol<<8) | (spix<<1) | pix
  uint16_t dcol = 4, spix = 4, pix = 0;
  uint16_t pixel_addr = (dcol << 8) | (spix << 1) | pix;  // = 0x0408
  uint16_t toa = 1000;
  uint16_t spidr_time = 2000;
  uint64_t hit_packet = createHitPacket(pixel_addr, toa, spidr_time);

  TPX3Packet packet(hit_packet);
  uint8_t chip_id = 1;                  // Test chip 1 mapping
  uint32_t tdc_timestamp = 0x10000000;  // Reference TDC timestamp

  TDCHit hit = convertPacketToHit(packet, chip_id, tdc_timestamp, *config);

  // Calculate expected local coordinates
  auto [local_x, local_y] = packet.getPixelCoordinates();

  // Check global coordinate mapping for chip 1 (from Python reference)
  // chip 1: x = 255 - local_x + 258, y = 255 - local_y + 258
  uint16_t expected_global_x = 255 - 4 + 258;  // = 509
  uint16_t expected_global_y = 255 - 4 + 258;  // = 509

  EXPECT_EQ(local_x, 4);  // dcol=4 + (pix>>2)=0 = 4
  EXPECT_EQ(local_y, 4);  // spix=4 + (pix&3)=0 = 4

  EXPECT_EQ(hit.x, expected_global_x);
  EXPECT_EQ(hit.y, expected_global_y);
  EXPECT_EQ(hit.chip_id, chip_id);
}

// Test 3: Should apply missing TDC correction algorithm
TEST_F(TDCHitTest, AppliesMissingTDCCorrection) {
  // RED PHASE: This will fail initially

  // Test the critical missing TDC correction algorithm
  // From Python: if TOF*25/1e9 > 1/TDC_frequency: TOF = TOF -
  // (1/TDC_frequency)*1e9/25

  // Create a scenario where TDC correction is needed
  // Use timestamps that result in TOF > 16.67ms (1/60Hz)
  uint32_t tdc_timestamp = 0x1000000;  // TDC timestamp
  // Create hit timestamp that gives TOF > TDC period
  uint32_t hit_timestamp =
      tdc_timestamp + 1000000;  // TOF = 1M * 25ns = 25ms > 16.67ms
  double tdc_frequency = 60.0;  // VENUS default

  // Calculate initial TOF
  uint32_t initial_tof = hit_timestamp - tdc_timestamp;  // = 1000000

  // Check if correction is needed: TOF * 25ns > 1/60Hz = 16.67ms
  double tof_seconds = initial_tof * 25e-9;  // Convert to seconds
  double tdc_period = 1.0 / tdc_frequency;   // = 0.01667 seconds
  bool needs_correction = tof_seconds > tdc_period;

  EXPECT_TRUE(needs_correction);  // This TOF is too large

  if (needs_correction) {
    // Apply correction: subtract one TDC period in 25ns units
    // Use proper rounding to match implementation
    uint32_t correction =
        static_cast<uint32_t>(tdc_period * 1e9 / 25 + 0.5);  // = 666667
    uint32_t corrected_tof = initial_tof - correction;

    // Create a packet with the calculated hit timestamp
    // We need to reverse engineer SPIDR and ToA from our hit_timestamp
    // hit_timestamp = (spidr_time << 14) | toa
    uint16_t test_spidr = hit_timestamp >> 14;   // Upper 16 bits
    uint16_t test_toa = hit_timestamp & 0x3FFF;  // Lower 14 bits
    uint64_t test_packet_data = createHitPacket(0x0408, test_toa, test_spidr);
    TPX3Packet test_packet(test_packet_data);
    uint8_t chip_id = 0;

    TDCHit hit =
        convertPacketToHit(test_packet, chip_id, tdc_timestamp, *config);
    EXPECT_EQ(hit.tof, corrected_tof);

    // For now, verify the calculation
    EXPECT_EQ(correction, 666667);  // One TDC period in 25ns units
    EXPECT_LT(corrected_tof, initial_tof);
  }
}

// Test 4: Should handle rollover detection and correction
TEST_F(TDCHitTest, HandlesRolloverDetection) {
  // RED PHASE: This will fail initially

  // Test rollover detection from Python reference:
  // if Timestamp25ns + 0x400000 < TDC_Timestamp25ns[chip]:
  //     Timestamp25ns |= 0x40000000

  uint16_t spidr_time = 0x1000;  // Small SPIDR time
  uint16_t toa = 0x1000;         // Small ToA
  uint64_t hit_packet = createHitPacket(0, toa, spidr_time);

  TPX3Packet packet(hit_packet);
  uint32_t hit_timestamp = packet.getTimestamp25ns();  // Small timestamp
  uint32_t tdc_timestamp = 0x3F000000;                 // Large TDC timestamp

  // Test rollover condition
  bool needs_rollover = (hit_timestamp + 0x400000) < tdc_timestamp;
  EXPECT_TRUE(needs_rollover);

  if (needs_rollover) {
    uint32_t extended_timestamp = hit_timestamp | 0x40000000;

    uint64_t test_packet_data = createHitPacket(0, toa, spidr_time);
    TPX3Packet test_packet_obj(test_packet_data);

    TDCHit hit = convertPacketToHit(test_packet_obj, 0, tdc_timestamp, *config);
    EXPECT_EQ(hit.timestamp, extended_timestamp);

    // For now, verify the extension
    EXPECT_NE(extended_timestamp, hit_timestamp);
    EXPECT_EQ(extended_timestamp & 0x40000000, 0x40000000);
  }
}

// Test 5: Should use detector configuration for chip mapping
TEST_F(TDCHitTest, UsesDetectorConfigForChipMapping) {
  // RED PHASE: This will fail initially

  // Test that chip mapping uses DetectorConfig values, not hardcoded
  // Use coordinates that will encode correctly within bit field limits
  // spix field: (addr & 0x1F8) >> 1, so max spix value is 0x1F8 >> 1 = 252 >> 1
  // = 126
  uint16_t local_x = 50;  // dcol=50, pix=0 -> fits in dcol field
  uint16_t local_y =
      72;  // spix=72, pix=0 -> 72*2=144, fits perfectly in spix field
  uint8_t chip_id = 1;

  // Get expected mapping from DetectorConfig
  auto [expected_global_x, expected_global_y] =
      config->mapChipToGlobal(chip_id, local_x, local_y);

  // Verify this matches Python reference for chip 1
  // Python: x = 255 - local_x + 258, y = 255 - local_y + 258
  uint16_t python_x = 255 - local_x + 258;  // 255 - 50 + 258 = 463
  uint16_t python_y = 255 - local_y + 258;  // 255 - 72 + 258 = 441

  EXPECT_EQ(expected_global_x, python_x);
  EXPECT_EQ(expected_global_y, python_y);

  // Test with actual conversion function
  // Create pixel address for coordinates (50, 72)
  // We need to reverse engineer: local_x = dcol + (pix >> 2), local_y = spix +
  // (pix & 0x3) For local (50, 72): dcol=50, spix=72, pix=0
  uint16_t dcol = 50, spix = 72, pix = 0;
  uint16_t pixel_addr = (dcol << 8) | (spix << 1) | pix;
  uint64_t test_packet = createHitPacket(pixel_addr, 1000, 2000);
  TPX3Packet packet(test_packet);
  uint32_t tdc_timestamp = 0x10000000;

  TDCHit hit = convertPacketToHit(packet, chip_id, tdc_timestamp, *config);

  EXPECT_EQ(hit.x, expected_global_x);
  EXPECT_EQ(hit.y, expected_global_y);
}

// Test 6: Should handle all chip IDs correctly
TEST_F(TDCHitTest, HandlesAllChipIDsCorrectly) {
  // RED PHASE: This will fail initially

  // Test mapping for all 4 chips (including chip 3 which Python doesn't handle)
  uint16_t local_x = 50;
  uint16_t local_y = 72;  // Use 72 instead of 75 to fit bit field limits

  // Test each chip mapping
  for (uint8_t chip_id = 0; chip_id < 4; ++chip_id) {
    auto [global_x, global_y] =
        config->mapChipToGlobal(chip_id, local_x, local_y);

    // Verify we get valid coordinates
    EXPECT_LT(global_x, 1000);  // Reasonable bounds
    EXPECT_LT(global_y, 1000);

    // Create pixel address for local coordinates (50, 72)
    uint16_t dcol = 50, spix = 72, pix = 0;
    uint16_t pixel_addr = (dcol << 8) | (spix << 1) | pix;
    uint64_t test_packet = createHitPacket(pixel_addr, 1000, 2000);
    TPX3Packet packet(test_packet);
    uint32_t tdc_timestamp = 0x10000000;

    TDCHit hit = convertPacketToHit(packet, chip_id, tdc_timestamp, *config);
    EXPECT_EQ(hit.x, global_x);
    EXPECT_EQ(hit.y, global_y);
    EXPECT_EQ(hit.chip_id, chip_id);
  }

  // For now, just verify the config mapping works
  EXPECT_TRUE(true);
}

}  // namespace tdcsophiread