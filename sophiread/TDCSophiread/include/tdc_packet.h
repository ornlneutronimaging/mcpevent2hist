// TDCSophiread TPX3 Packet Parser
// TDC-only packet parsing for TPX3 data format
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <cstdint>
#include <tuple>

namespace tdcsophiread {

/**
 * @brief TPX3 packet parser for TDC-only processing
 *
 * Handles three types of packets in TPX3 data stream:
 * 1. TPX3 Header: Contains chip ID (magic: 0x33585054 = "TPX3")
 * 2. TDC Packet: Contains 30-bit timestamp reference (ID: 0x6F)
 * 3. Hit Packet: Contains pixel data and timing (ID: 0xB*)
 *
 * All timestamps are in 25ns units (LSB = 25 nanoseconds)
 */
class TPX3Packet {
 public:
  /**
   * @brief Construct packet from raw 64-bit data
   * @param raw_packet 64-bit packet data from TPX3 stream
   */
  explicit TPX3Packet(uint64_t raw_packet) : m_RawPacket(raw_packet) {}

  // Packet type identification
  bool isTPX3Header() const { return (m_RawPacket & 0xFFFFFFFF) == 0x33585054; }

  bool isTDC() const { return ((m_RawPacket >> 56) & 0xFF) == 0x6F; }

  bool isHit() const { return ((m_RawPacket >> 60) & 0xF) == 0xB; }

  // TPX3 Header accessors
  uint8_t getChipID() const { return (m_RawPacket >> 32) & 0xFF; }

  // TDC packet accessors
  uint32_t getTDCTimestamp() const {
    // 30-bit timestamp in bits 12-41
    return (m_RawPacket >> 12) & 0x3FFFFFFF;
  }

  // Hit packet accessors
  uint16_t getPixelAddress() const {
    // 16-bit pixel address in bits 44-59
    return (m_RawPacket >> 44) & 0xFFFF;
  }

  uint16_t getToA() const {
    // 14-bit Time of Arrival in bits 30-43
    return (m_RawPacket >> 30) & 0x3FFF;
  }

  uint16_t getSPIDRTime() const {
    // 16-bit SPIDR time in bits 0-15
    return m_RawPacket & 0xFFFF;
  }

  uint16_t getToT() const {
    // 10-bit Time over Threshold in bits 20-29
    return (m_RawPacket >> 20) & 0x3FF;
  }

  uint8_t getFToA() const {
    // 4-bit fine Time of Arrival in bits 16-19
    return (m_RawPacket >> 16) & 0xF;
  }

  /**
   * @brief Extract pixel coordinates from address
   *
   * Based on Python reference:
   * - dcol = (addr & 0xFE00) >> 8
   * - spix = (addr & 0x1F8) >> 1
   * - pix = addr & 0x7
   * - x = dcol + (pix >> 2)
   * - y = spix + (pix & 0x3)
   *
   * @return std::tuple<uint16_t, uint16_t> Local chip coordinates (x, y)
   */
  std::tuple<uint16_t, uint16_t> getPixelCoordinates() const {
    uint16_t addr = getPixelAddress();
    uint16_t dcol = (addr & 0xFE00) >> 8;
    uint16_t spix = (addr & 0x1F8) >> 1;
    uint16_t pix = addr & 0x7;

    uint16_t x = dcol + (pix >> 2);
    uint16_t y = spix + (pix & 0x3);

    return {x, y};
  }

  /**
   * @brief Get combined 30-bit timestamp for hit packets
   *
   * Combines SPIDR time (16 bits) and ToA (14 bits) into 30-bit timestamp
   * Formula: (SPIDRtime << 14) | ToA
   *
   * @return uint32_t 30-bit timestamp in 25ns units
   */
  uint32_t getTimestamp25ns() const {
    return (getSPIDRTime() << 14) | getToA();
  }

  /**
   * @brief Get raw packet data
   * @return uint64_t Raw 64-bit packet
   */
  uint64_t getRawPacket() const { return m_RawPacket; }

 private:
  uint64_t m_RawPacket;  ///< Raw 64-bit packet data
};

}  // namespace tdcsophiread