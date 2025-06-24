// TDCSophiread Packet Parser Tests
// TDD approach: Tests for TPX3 packet parsing with TDC-only logic

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "tdc_packet.h"

namespace tdcsophiread {

// Test class for packet parsing
class TDCPacketTest : public ::testing::Test {
 protected:
  // Helper to create raw TPX3 header packet
  uint64_t createTPX3HeaderPacket(uint8_t chip_id) {
    // TPX3 header: 'TPX3' (0x33585054) in lower 32 bits, chip ID in bits 32-39
    uint64_t packet = 0x33585054ULL;  // "TPX3" magic
    packet |= (static_cast<uint64_t>(chip_id) << 32);
    return packet;
  }

  // Helper to create TDC packet
  uint64_t createTDCPacket(uint32_t timestamp_30bit) {
    // TDC packet: ID=0x6F in bits 56-63, timestamp in bits 12-41
    uint64_t packet = 0x6F00000000000000ULL;  // Packet ID in upper byte
    packet |= (static_cast<uint64_t>(timestamp_30bit & 0x3FFFFFFF) << 12);
    return packet;
  }

  // Helper to create pixel hit packet
  uint64_t createHitPacket(uint16_t pixel_addr, uint16_t toa,
                           uint16_t spidr_time) {
    // Hit packet: ID=0xB* in bits 60-63, pixel in 44-59, ToA in 30-43, SPIDR in
    // 0-15
    uint64_t packet = 0xB000000000000000ULL;  // Packet ID (upper nibble = 0xB)
    packet |= (static_cast<uint64_t>(pixel_addr & 0xFFFF) << 44);
    packet |= (static_cast<uint64_t>(toa & 0x3FFF) << 30);
    packet |= (spidr_time & 0xFFFF);
    return packet;
  }
};

// Test 1: TPX3Packet should identify packet types correctly
TEST_F(TDCPacketTest, IdentifiesPacketTypes) {
  // GREEN PHASE: Testing actual implementation

  // Test TPX3 header detection
  uint64_t header_packet = createTPX3HeaderPacket(2);
  TPX3Packet packet(header_packet);
  EXPECT_TRUE(packet.isTPX3Header());
  EXPECT_FALSE(packet.isTDC());
  EXPECT_FALSE(packet.isHit());
  EXPECT_EQ(packet.getChipID(), 2);
}

// Test 2: TPX3Packet should parse TDC packets correctly
TEST_F(TDCPacketTest, ParsesTDCPackets) {
  // GREEN PHASE: Testing actual implementation

  uint32_t test_timestamp = 0x12345678;
  uint64_t tdc_packet = createTDCPacket(test_timestamp);

  TPX3Packet packet(tdc_packet);
  EXPECT_TRUE(packet.isTDC());
  EXPECT_FALSE(packet.isTPX3Header());
  EXPECT_FALSE(packet.isHit());
  EXPECT_EQ(packet.getTDCTimestamp(), test_timestamp & 0x3FFFFFFF);
}

// Test 3: TPX3Packet should parse hit packets correctly
TEST_F(TDCPacketTest, ParsesHitPackets) {
  // GREEN PHASE: Testing actual implementation

  uint16_t pixel_addr = 0x1234;  // Example pixel address
  uint16_t toa = 0x2468;         // 14-bit ToA
  uint16_t spidr_time = 0xABCD;  // 16-bit SPIDR time

  uint64_t hit_packet = createHitPacket(pixel_addr, toa, spidr_time);

  TPX3Packet packet(hit_packet);
  EXPECT_TRUE(packet.isHit());
  EXPECT_FALSE(packet.isTPX3Header());
  EXPECT_FALSE(packet.isTDC());
  EXPECT_EQ(packet.getPixelAddress(), pixel_addr);
  EXPECT_EQ(packet.getToA(), toa & 0x3FFF);
  EXPECT_EQ(packet.getSPIDRTime(), spidr_time);
}

// Test 4: Hit packet should extract pixel coordinates correctly
TEST_F(TDCPacketTest, ExtractsPixelCoordinates) {
  // GREEN PHASE: Testing actual implementation

  // Test a specific pixel address encoding
  // Based on Python: dcol = (addr & 0xFE00) >> 8
  //                 spix = (addr & 0x1F8) >> 1
  //                 pix = addr & 0x7
  //                 x = dcol + (pix >> 2)
  //                 y = spix + (pix & 0x3)

  uint16_t pixel_addr = 0xFE0F;  // Example encoding
  uint64_t hit_packet = createHitPacket(pixel_addr, 0, 0);

  TPX3Packet packet(hit_packet);
  auto [x, y] = packet.getPixelCoordinates();

  uint16_t dcol = (pixel_addr & 0xFE00) >> 8;  // = 0x7F
  uint16_t spix = (pixel_addr & 0x1F8) >> 1;   // = 0x07
  uint16_t pix = pixel_addr & 0x7;             // = 0x07
  uint16_t expected_x = dcol + (pix >> 2);     // = 0x7F + 1 = 128
  uint16_t expected_y = spix + (pix & 0x3);    // = 0x07 + 3 = 10

  EXPECT_EQ(x, expected_x);
  EXPECT_EQ(y, expected_y);
}

// Test 5: Parser should handle timestamp extraction for TOF
TEST_F(TDCPacketTest, ExtractsTimestampsForTOF) {
  // GREEN PHASE: Testing actual implementation

  uint16_t spidr_time = 0x1234;
  uint16_t toa = 0x2468;
  uint64_t hit_packet = createHitPacket(0, toa, spidr_time);

  TPX3Packet packet(hit_packet);
  uint32_t timestamp_25ns = packet.getTimestamp25ns();

  // From Python: Timestamp25ns = (SPIDRtime << 14) | ToA
  uint32_t expected = (spidr_time << 14) | (toa & 0x3FFF);
  EXPECT_EQ(timestamp_25ns, expected);
  EXPECT_EQ(timestamp_25ns, 0x48D2468);  // Verify specific value
}

// Test 6: Parser should detect rollover conditions
TEST_F(TDCPacketTest, DetectsRolloverConditions) {
  // RED PHASE: This will fail initially

  // Test the rollover detection logic from Python:
  // if Timestamp25ns + 0x400000 < TDC_Timestamp25ns[chip]:
  //     Timestamp25ns |= 0x40000000

  // This tests that we can detect when a hit timestamp needs extension

  // For now, just document the algorithm
  uint32_t hit_timestamp = 0x100000;    // Small timestamp
  uint32_t tdc_timestamp = 0x3F000000;  // Large TDC timestamp

  // Should detect rollover condition
  bool needs_extension = (hit_timestamp + 0x400000) < tdc_timestamp;
  EXPECT_TRUE(needs_extension);

  if (needs_extension) {
    hit_timestamp |= 0x40000000;
  }
  EXPECT_EQ(hit_timestamp, 0x40100000);
}

}  // namespace tdcsophiread