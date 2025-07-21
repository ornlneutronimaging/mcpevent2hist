// TDCSophiread Processor Tests - TDD Approach
// These tests define the expected behavior for section-aware TPX3 processing

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include "tdc_detector_config.h"
#include "tdc_hit.h"
#include "tdc_io.h"
#include "tdc_processor.h"

namespace tdcsophiread {

// Test class for TDCProcessor
class TDCProcessorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create temporary directory for test files
    test_dir = std::filesystem::temp_directory_path() / "tdc_test_processor";
    std::filesystem::create_directories(test_dir);

    // Use VENUS defaults for testing
    config = std::make_unique<DetectorConfig>(DetectorConfig::venusDefaults());
  }

  void TearDown() override {
    // Clean up test files
    if (std::filesystem::exists(test_dir)) {
      std::filesystem::remove_all(test_dir);
    }
  }

  // Helper to create test TPX3 file with known data
  void createTestTPX3File(const std::string& filename,
                          const std::vector<uint64_t>& packets) {
    std::ofstream file(test_dir / filename, std::ios::binary);
    for (const auto& packet : packets) {
      file.write(reinterpret_cast<const char*>(&packet), sizeof(packet));
    }
    file.close();
  }

  // Helper to create TPX3 header packet
  uint64_t createTPX3HeaderPacket(uint8_t chip_id) {
    // From Python: if (packet & 0xFFFFFFFF) == 0x33585054
    uint64_t packet = 0x33585054;                      // "TPX3" magic number
    packet |= (static_cast<uint64_t>(chip_id) << 32);  // Chip ID in bits 39-32
    return packet;
  }

  // Helper to create TDC packet
  uint64_t createTDCPacket(uint32_t timestamp_30bit) {
    // From Python: if PacketID == 0x6F (bits 63-56 = 0x6)
    uint64_t packet = 0x6F00000000000000ULL;  // TDC packet ID
    // Python: TDC_Timestamp25ns[chip] = (packet >> 12) & 0x3FFFFFFF
    packet |= (static_cast<uint64_t>(timestamp_30bit & 0x3FFFFFFF) << 12);
    return packet;
  }

  // Helper to create hit packet
  uint64_t createHitPacket(uint16_t pixel_addr, uint16_t toa,
                           uint16_t spidr_time, uint16_t tot = 100) {
    // From Python: elif ((PacketID >> 4) == 0xB)
    uint64_t packet =
        0xB000000000000000ULL;  // Hit packet ID (bits 63-60 = 0xB)
    // Python: Pixaddr = (packet >> 44) & 0xFFFF
    packet |= (static_cast<uint64_t>(pixel_addr & 0xFFFF) << 44);
    // Python: ToA = (packet >> 30) & 0x3FFF
    packet |= (static_cast<uint64_t>(toa & 0x3FFF) << 30);
    // Python: SPIDRtime = packet & 0xFFFF
    packet |= (spidr_time & 0xFFFF);
    // TOT in bits 29-20
    packet |= (static_cast<uint64_t>(tot & 0x3FF) << 20);
    return packet;
  }

  std::filesystem::path test_dir;
  std::unique_ptr<DetectorConfig> config;
};

// Test 1: Section discovery should find TPX3 headers
TEST_F(TDCProcessorTest, DiscoversSectionsBasedOnTPX3Headers) {
  // Define expected behavior: Processor should identify sections between TPX3
  // headers

  // Create file with multiple sections
  std::vector<uint64_t> packets = {
      createTPX3HeaderPacket(0),  // Section 1 start (chip 0)
      createTDCPacket(1000),
      createHitPacket(0x0408, 100, 200),
      createHitPacket(0x0409, 101, 201),
      createTPX3HeaderPacket(1),  // Section 2 start (chip 1)
      createTDCPacket(2000),
      createHitPacket(0x040A, 102, 202),
      createTPX3HeaderPacket(0),  // Section 3 start (chip 0 again)
      createTDCPacket(3000),
      createHitPacket(0x040B, 103, 203)};

  createTestTPX3File("sections.tpx3", packets);
  std::string file_path = (test_dir / "sections.tpx3").string();

  TDCProcessor processor(*config);

  // Open file and use mapped data for section discovery
  auto mapped_file = MappedFile::open(file_path);
  auto sections =
      processor.discoverSections(mapped_file->data(), mapped_file->size());

  EXPECT_EQ(sections.size(), 3);

  // Verify section details
  EXPECT_EQ(sections[0].chip_id, 0);
  EXPECT_EQ(sections[0].start_offset, 0);
  EXPECT_EQ(sections[0].end_offset, 4 * 8);  // 4 packets * 8 bytes

  EXPECT_EQ(sections[1].chip_id, 1);
  EXPECT_EQ(sections[1].start_offset, 4 * 8);
  EXPECT_EQ(sections[1].end_offset, 7 * 8);

  EXPECT_EQ(sections[2].chip_id, 0);
  EXPECT_EQ(sections[2].start_offset, 7 * 8);
  EXPECT_EQ(sections[2].end_offset, 10 * 8);
}

// Test 2: Single-threaded processing should respect section boundaries
TEST_F(TDCProcessorTest, ProcessesSectionsSequentiallyWithTDCPropagation) {
  // Define expected behavior: TDC timestamps should propagate across sections
  // of same chip

  std::vector<uint64_t> packets = {
      // Section 1: Chip 0
      createTPX3HeaderPacket(0), createTDCPacket(1000),
      createHitPacket(0x0408, 100, 200),  // TOF = (200<<14|100) - 1000
      // Section 2: Chip 1
      createTPX3HeaderPacket(1), createTDCPacket(2000),
      createHitPacket(0x0409, 101, 201),
      // Section 3: Chip 0 (should inherit TDC from Section 1)
      createTPX3HeaderPacket(0),
      // No new TDC packet! Should use TDC=1000 from previous chip 0 section
      createHitPacket(0x040A, 102, 202)};

  createTestTPX3File("tdc_propagation.tpx3", packets);
  std::string file_path = (test_dir / "tdc_propagation.tpx3").string();

  TDCProcessor processor(*config);
  processor.setMissingTdcCorrectionEnabled(
      false);  // Disable correction for raw TOF
  auto hits = processor.processFile(file_path);

  EXPECT_EQ(hits.size(), 3);

  // Verify first hit (chip 0)
  uint32_t timestamp1 = (200 << 14) | 100;
  EXPECT_EQ(hits[0].tof, timestamp1 - 1000);
  EXPECT_EQ(hits[0].chip_id, 0);

  // Verify second hit (chip 1)
  uint32_t timestamp2 = (201 << 14) | 101;
  EXPECT_EQ(hits[1].tof, timestamp2 - 2000);
  EXPECT_EQ(hits[1].chip_id, 1);

  // Verify third hit (chip 0 - uses inherited TDC)
  uint32_t timestamp3 = (202 << 14) | 102;
  EXPECT_EQ(hits[2].tof,
            timestamp3 - 1000);  // Uses TDC=1000 from first section
  EXPECT_EQ(hits[2].chip_id, 0);
}

// Test 3: Missing TDC correction should be applied
TEST_F(TDCProcessorTest, AppliesMissingTDCCorrectionLikePythonReference) {
  // From Python: if TOF*25/1e9 > 1/TDC_frequency: TOF = TOF -
  // (1/TDC_frequency)*1e9/25

  uint32_t tdc_timestamp = 0x1000000;
  // Create TOF > 16.67ms (1/60Hz)
  uint32_t large_tof_25ns = 700000;  // 17.5ms in 25ns units
  uint32_t hit_timestamp = tdc_timestamp + large_tof_25ns;

  // Reverse engineer spidr_time and toa from hit timestamp
  uint16_t spidr_time = hit_timestamp >> 14;
  uint16_t toa = hit_timestamp & 0x3FFF;

  std::vector<uint64_t> packets = {createTPX3HeaderPacket(0),
                                   createTDCPacket(tdc_timestamp),
                                   createHitPacket(0x0408, toa, spidr_time)};

  createTestTPX3File("tdc_correction.tpx3", packets);
  std::string file_path = (test_dir / "tdc_correction.tpx3").string();

  TDCProcessor processor(*config);
  auto hits = processor.processFile(file_path);

  EXPECT_EQ(hits.size(), 1);

  // Calculate expected corrected TOF
  double tdc_period = 1.0 / config->getTdcFrequency();  // 1/60 = 0.01667s
  uint32_t correction_25ns = static_cast<uint32_t>(tdc_period * 1e9 / 25 + 0.5);
  uint32_t expected_corrected_tof = large_tof_25ns - correction_25ns;

  EXPECT_EQ(hits[0].tof, expected_corrected_tof);
}

// Test 4: Timestamp rollover should be detected and handled
TEST_F(TDCProcessorTest, HandlesTimestampRolloverLikePythonReference) {
  // From Python: if Timestamp25ns + 0x400000 <
  // TDC_Timestamp25ns[TPXnumber_current]:
  //               Timestamp25ns = Timestamp25ns | 0x40000000

  uint32_t tdc_timestamp = 0x3FFF0000;  // High TDC timestamp
  uint16_t spidr_time = 0x0001;         // Low SPIDR time (rolled over)
  uint16_t toa = 0x0100;

  std::vector<uint64_t> packets = {createTPX3HeaderPacket(0),
                                   createTDCPacket(tdc_timestamp),
                                   createHitPacket(0x0408, toa, spidr_time)};

  createTestTPX3File("rollover.tpx3", packets);
  std::string file_path = (test_dir / "rollover.tpx3").string();

  TDCProcessor processor(*config);
  auto hits = processor.processFile(file_path);

  EXPECT_EQ(hits.size(), 1);

  // Verify rollover was detected and corrected
  uint32_t raw_timestamp = (spidr_time << 14) | toa;
  uint32_t rollover_check = raw_timestamp + 0x400000;

  EXPECT_LT(rollover_check, tdc_timestamp);  // Confirms rollover condition

  // TOF should be calculated with rollover bit set
  uint32_t corrected_timestamp = raw_timestamp | 0x40000000;
  uint32_t expected_tof = corrected_timestamp - tdc_timestamp;

  EXPECT_EQ(hits[0].tof, expected_tof);
}

// Test 5: Coordinate mapping should match Python reference
TEST_F(TDCProcessorTest, MapsCoordinatesAccordingToPythonReference) {
  // Test coordinate mapping for all 4 chips based on Python code

  uint32_t tdc_timestamp = 0x1000000;
  uint16_t pixel_addr = 0x0408;  // dcol=4, spix=4, pix=0

  std::vector<uint64_t> packets;

  // Add hits for all 4 chips
  for (uint8_t chip = 0; chip < 4; ++chip) {
    packets.push_back(createTPX3HeaderPacket(chip));
    packets.push_back(createTDCPacket(tdc_timestamp));
    packets.push_back(createHitPacket(pixel_addr, 1000 + chip, 2000 + chip));
  }

  createTestTPX3File("coordinate_mapping.tpx3", packets);
  std::string file_path = (test_dir / "coordinate_mapping.tpx3").string();

  TDCProcessor processor(*config);
  auto hits = processor.processFile(file_path);

  EXPECT_EQ(hits.size(), 4);

  // From Python: m_x = dcol + (pix >> 2) = 4 + 0 = 4
  //              m_y = spix + (pix & 0x3) = 4 + 0 = 4

  // Chip 0: m_x += 258
  EXPECT_EQ(hits[0].x, 4 + 258);
  EXPECT_EQ(hits[0].y, 4);

  // Chip 1: m_x = 255 - m_x + 258, m_y = 255 - m_y + 258
  EXPECT_EQ(hits[1].x, 255 - 4 + 258);
  EXPECT_EQ(hits[1].y, 255 - 4 + 258);

  // Chip 2: m_x = 255 - m_x, m_y = 255 - m_y + 258
  EXPECT_EQ(hits[2].x, 255 - 4);
  EXPECT_EQ(hits[2].y, 255 - 4 + 258);

  // Chip 3: m_x unchanged, m_y unchanged (per Python reference)
  EXPECT_EQ(hits[3].x, 4);
  EXPECT_EQ(hits[3].y, 4);
}

// Test 6: Chunk-based processing produces consistent results
TEST_F(TDCProcessorTest, ChunkBasedProcessingConsistentResults) {
  // Test that chunk-based processing produces same results as before
  // but with configurable memory usage

  std::vector<uint64_t> packets = {
      // Section 1: 32 bytes (4 packets)
      createTPX3HeaderPacket(0), createTDCPacket(1000),
      createHitPacket(0x0408, 100, 200), createHitPacket(0x0409, 101, 201),
      // Section 2: 24 bytes (3 packets)
      createTPX3HeaderPacket(1), createTDCPacket(2000),
      createHitPacket(0x040A, 102, 202),
      // Section 3: 24 bytes (3 packets)
      createTPX3HeaderPacket(2), createTDCPacket(3000),
      createHitPacket(0x040B, 103, 203)};

  createTestTPX3File("chunk_processing.tpx3", packets);
  std::string file_path = (test_dir / "chunk_processing.tpx3").string();

  TDCProcessor processor(*config);
  processor.setMissingTdcCorrectionEnabled(
      false);  // Disable for consistent results

  // Process with different chunk sizes - should get same results
  auto hits_large = processor.processFile(file_path, 1024);  // 1GB chunks
  auto hits_small = processor.processFile(file_path, 1);     // 1MB chunks

  // Results should be identical regardless of chunk size
  EXPECT_EQ(hits_large.size(), hits_small.size());
  EXPECT_EQ(hits_large.size(), 4);  // All 4 hits processed

  // Content should be identical
  for (size_t i = 0; i < hits_large.size(); ++i) {
    EXPECT_EQ(hits_large[i].x, hits_small[i].x);
    EXPECT_EQ(hits_large[i].y, hits_small[i].y);
    EXPECT_EQ(hits_large[i].tof, hits_small[i].tof);
  }
}

// Test 7: Performance baseline for single-threaded processing
TEST_F(TDCProcessorTest, AchievesBaselinePerformanceSingleThreaded) {
  // Create file with 1 million hits to test performance

  uint32_t tdc_timestamp = 0x10000000;
  std::vector<uint64_t> packets;

  // Add TPX3 header and initial TDC
  packets.push_back(createTPX3HeaderPacket(0));
  packets.push_back(createTDCPacket(tdc_timestamp));

  // Add 1 million hit packets
  for (int i = 0; i < 1000000; ++i) {
    uint16_t pixel_addr = (i % 65536);
    uint16_t toa = 1000 + (i % 16384);
    uint16_t spidr_time = 2000 + (i % 65536);
    packets.push_back(createHitPacket(pixel_addr, toa, spidr_time));

    // Add TDC update every 10k hits
    if (i % 10000 == 9999) {
      tdc_timestamp += 100000;
      packets.push_back(createTDCPacket(tdc_timestamp));
    }
  }

  createTestTPX3File("performance.tpx3", packets);
  std::string file_path = (test_dir / "performance.tpx3").string();

  TDCProcessor processor(*config);

  auto start_time = std::chrono::high_resolution_clock::now();
  auto hits = processor.processFile(file_path);
  auto end_time = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                      end_time - start_time)
                      .count();

  EXPECT_EQ(hits.size(), 1000000);

  // Calculate hits per second
  double hits_per_second = (hits.size() * 1e6) / duration;

  // Should achieve at least 10M hits/sec single-threaded (lowered for CI
  // variability) Note: Production systems typically achieve 20-50M hits/sec
  EXPECT_GT(hits_per_second, 10e6);

  // Verify processor metrics
  EXPECT_GT(processor.getLastHitsPerSecond(), 10e6);
  EXPECT_EQ(processor.getLastHitCount(), 1000000);
}

// Test 8: Empty file should be handled gracefully
TEST_F(TDCProcessorTest, HandlesEmptyFileGracefully) {
  createTestTPX3File("empty.tpx3", {});
  std::string file_path = (test_dir / "empty.tpx3").string();

  TDCProcessor processor(*config);
  auto hits = processor.processFile(file_path);

  EXPECT_TRUE(hits.empty());
  EXPECT_EQ(processor.getLastHitCount(), 0);
}

// Test 9: File with no hits (only TDC packets) should produce no hits
TEST_F(TDCProcessorTest, FileWithOnlyTDCPacketsProducesNoHits) {
  std::vector<uint64_t> packets = {createTPX3HeaderPacket(0),
                                   createTDCPacket(1000), createTDCPacket(2000),
                                   createTDCPacket(3000)};

  createTestTPX3File("tdc_only.tpx3", packets);
  std::string file_path = (test_dir / "tdc_only.tpx3").string();

  TDCProcessor processor(*config);
  auto hits = processor.processFile(file_path);

  EXPECT_TRUE(hits.empty());
}

// Test 10: Hits before first TDC should be ignored
TEST_F(TDCProcessorTest, IgnoresHitsBeforeFirstTDC) {
  // From Python: hits are only processed after TDC_found = True

  std::vector<uint64_t> packets = {
      createTPX3HeaderPacket(0),
      createHitPacket(0x0408, 100, 200),  // Should be ignored - no TDC yet
      createHitPacket(0x0409, 101, 201),  // Should be ignored
      createTDCPacket(1000),              // Now TDC is available
      createHitPacket(0x040A, 102, 202)   // This should be processed
  };

  createTestTPX3File("hits_before_tdc.tpx3", packets);
  std::string file_path = (test_dir / "hits_before_tdc.tpx3").string();

  TDCProcessor processor(*config);
  processor.setMissingTdcCorrectionEnabled(
      false);  // Disable correction for raw TOF test
  auto hits = processor.processFile(file_path);

  EXPECT_EQ(hits.size(), 1);  // Only the hit after TDC
  EXPECT_EQ(hits[0].tof, ((202 << 14) | 102) - 1000);
}

// Test 11: Parallel processing should produce identical results to sequential
TEST_F(TDCProcessorTest, ParallelProcessingMatchesSequential) {
  // Create a complex file with multiple sections and chips
  std::vector<uint64_t> packets;

  uint32_t base_tdc = 0x1000000;

  // Create 20 sections across all 4 chips with varying TDC timestamps
  for (int section = 0; section < 20; ++section) {
    uint8_t chip_id = section % 4;
    uint32_t tdc_timestamp = base_tdc + (section * 50000);

    packets.push_back(createTPX3HeaderPacket(chip_id));
    packets.push_back(createTDCPacket(tdc_timestamp));

    // Add multiple hits per section
    for (int hit = 0; hit < 100; ++hit) {
      uint16_t pixel_addr = 0x0400 + hit;
      uint16_t toa = 1000 + hit;
      uint16_t spidr_time = 2000 + hit;
      packets.push_back(createHitPacket(pixel_addr, toa, spidr_time));
    }
  }

  createTestTPX3File("parallel_test.tpx3", packets);
  std::string file_path = (test_dir / "parallel_test.tpx3").string();

  TDCProcessor processor(*config);
  processor.setMissingTdcCorrectionEnabled(false);  // For consistent comparison

  // Process with single-threaded method
  auto sequential_hits = processor.processFile(file_path, 512, false, 0);

  // Process with parallel method (using 4 threads)
  auto parallel_hits = processor.processFile(file_path, 512, true, 4);

  // Results should be identical in content (order may differ due to
  // parallelization)
  EXPECT_EQ(sequential_hits.size(), parallel_hits.size());
  EXPECT_EQ(sequential_hits.size(), 20 * 100);  // 20 sections * 100 hits each

  // Sort both vectors by a composite key for comparison
  auto hit_comparator = [](const TDCHit& a, const TDCHit& b) {
    if (a.chip_id != b.chip_id) return a.chip_id < b.chip_id;
    if (a.x != b.x) return a.x < b.x;
    if (a.y != b.y) return a.y < b.y;
    return a.tof < b.tof;
  };

  std::sort(sequential_hits.begin(), sequential_hits.end(), hit_comparator);
  std::sort(parallel_hits.begin(), parallel_hits.end(), hit_comparator);

  // Compare sorted results element by element
  for (size_t i = 0; i < sequential_hits.size(); ++i) {
    EXPECT_EQ(sequential_hits[i].x, parallel_hits[i].x);
    EXPECT_EQ(sequential_hits[i].y, parallel_hits[i].y);
    EXPECT_EQ(sequential_hits[i].tof, parallel_hits[i].tof);
    EXPECT_EQ(sequential_hits[i].chip_id, parallel_hits[i].chip_id);
    EXPECT_EQ(sequential_hits[i].tot, parallel_hits[i].tot);
    EXPECT_EQ(sequential_hits[i].timestamp, parallel_hits[i].timestamp);
  }
}

// Test 12: Parallel processing performance benchmark
TEST_F(TDCProcessorTest, ParallelProcessingAchievesTargetPerformance) {
  // Create large file with 1 million hits across multiple sections
  std::vector<uint64_t> packets;

  uint32_t base_tdc = 0x10000000;
  const int hits_per_section = 10000;
  const int num_sections = 100;  // Total: 1M hits

  for (int section = 0; section < num_sections; ++section) {
    uint8_t chip_id = section % 4;
    uint32_t tdc_timestamp = base_tdc + (section * 100000);

    packets.push_back(createTPX3HeaderPacket(chip_id));
    packets.push_back(createTDCPacket(tdc_timestamp));

    for (int hit = 0; hit < hits_per_section; ++hit) {
      uint16_t pixel_addr = (hit % 65536);
      uint16_t toa = 1000 + (hit % 16384);
      uint16_t spidr_time = 2000 + (hit % 65536);
      packets.push_back(createHitPacket(pixel_addr, toa, spidr_time));
    }
  }

  createTestTPX3File("parallel_performance.tpx3", packets);
  std::string file_path = (test_dir / "parallel_performance.tpx3").string();

  TDCProcessor processor(*config);

  // Benchmark parallel processing with multiple thread counts
  auto start_time = std::chrono::high_resolution_clock::now();
  auto hits =
      processor.processFile(file_path, 512, true, 0);  // Auto-detect threads
  auto end_time = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                      end_time - start_time)
                      .count();

  EXPECT_EQ(hits.size(), num_sections * hits_per_section);

  // Calculate hits per second
  double hits_per_second = (hits.size() * 1e6) / duration;

  // Performance reporting (disabled for CI due to unstable shared hardware)
  // Target: 120M hits/sec on production hardware
  // CI environments vary widely: 20M-100M+ hits/sec depending on load
  std::cout << "Parallel processing performance: " << hits_per_second / 1e6
            << " M hits/sec" << std::endl;

  // Verify basic functionality instead of performance
  EXPECT_GT(hits_per_second, 1e6);  // At least 1M hits/sec (very conservative)

  // Verify processor metrics are populated correctly
  EXPECT_GT(processor.getLastHitsPerSecond(), 1e6);
  EXPECT_EQ(processor.getLastHitCount(), num_sections * hits_per_section);
}

}  // namespace tdcsophiread