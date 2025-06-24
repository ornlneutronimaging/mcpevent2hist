// TDCSophiread I/O Tests
// TDD approach: Tests for memory-mapped file I/O (Linux & macOS only)

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

#include "tdc_io.h"

namespace tdcsophiread {

// Test class for I/O operations
class TDCIOTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create temporary directory for test files
    test_dir_ = std::filesystem::temp_directory_path() / "tdc_test_io";
    std::filesystem::create_directories(test_dir_);
  }

  void TearDown() override {
    // Clean up test files
    if (std::filesystem::exists(test_dir_)) {
      std::filesystem::remove_all(test_dir_);
    }
  }

  // Helper to create test binary files
  void createTestFile(const std::string& filename, size_t size_bytes) {
    std::ofstream file(test_dir_ / filename, std::ios::binary | std::ios::out);

    // Create test pattern - incrementing bytes
    std::vector<uint8_t> data(size_bytes);
    for (size_t i = 0; i < size_bytes; ++i) {
      data[i] = static_cast<uint8_t>(i % 256);
    }

    file.write(reinterpret_cast<const char*>(data.data()), size_bytes);
    file.close();
  }

  // Helper to create TPX3-like test file with header
  void createTPX3TestFile(const std::string& filename, size_t num_packets) {
    std::ofstream file(test_dir_ / filename, std::ios::binary | std::ios::out);

    // TPX3 header magic: "TPX3" = 0x33585054 in little-endian
    uint32_t header = 0x33585054;
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write some dummy 64-bit packets
    for (size_t i = 0; i < num_packets; ++i) {
      uint64_t packet = i;
      file.write(reinterpret_cast<const char*>(&packet), sizeof(packet));
    }

    file.close();
  }

  std::filesystem::path test_dir_;
};

// Test 1: MappedFile should open and map a valid file
TEST_F(TDCIOTest, OpensAndMapsValidFile) {
  // GREEN PHASE: Now testing actual implementation

  // Create a test file
  std::string filename = "test_data.bin";
  size_t file_size = 1024;  // 1KB
  createTestFile(filename, file_size);

  std::string filepath = (test_dir_ / filename).string();

  // Test the actual implementation
  auto mapped_file = MappedFile::open(filepath);

  EXPECT_TRUE(mapped_file);
  EXPECT_EQ(mapped_file->size(), file_size);
  EXPECT_NE(mapped_file->data(), nullptr);
  EXPECT_EQ(mapped_file->path(), filepath);
}

// Test 2: MappedFile should handle missing files gracefully
TEST_F(TDCIOTest, HandlesMissingFileGracefully) {
  // GREEN PHASE: Now testing actual implementation

  std::string nonexistent = (test_dir_ / "nonexistent.bin").string();

  // This should throw an exception
  EXPECT_THROW(MappedFile::open(nonexistent), std::runtime_error);
}

// Test 3: MappedFile should provide read-only access to data
TEST_F(TDCIOTest, ProvidesReadOnlyDataAccess) {
  // GREEN PHASE: Now testing actual implementation

  std::string filename = "readonly_test.bin";
  size_t file_size = 256;
  createTestFile(filename, file_size);

  std::string filepath = (test_dir_ / filename).string();

  auto mapped_file = MappedFile::open(filepath);

  const uint8_t* data = mapped_file->data();

  // Verify we can read the data
  for (size_t i = 0; i < 256; ++i) {
    EXPECT_EQ(data[i], static_cast<uint8_t>(i % 256));
  }
}

// Test 4: MappedFile should handle large files efficiently
TEST_F(TDCIOTest, HandlesLargeFilesEfficiently) {
  // GREEN PHASE: Now testing actual implementation

  std::string filename = "large_test.bin";
  size_t file_size = 10 * 1024 * 1024;  // 10MB
  createTestFile(filename, file_size);

  std::string filepath = (test_dir_ / filename).string();

  auto start = std::chrono::high_resolution_clock::now();
  auto mapped_file = MappedFile::open(filepath);
  auto end = std::chrono::high_resolution_clock::now();

  // Opening should be fast (< 100ms for memory mapping)
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  EXPECT_LT(duration.count(), 100);

  EXPECT_EQ(mapped_file->size(), file_size);
  EXPECT_NE(mapped_file->data(), nullptr);
}

// Test 5: MappedFile should work with TPX3 file format
TEST_F(TDCIOTest, WorksWithTPX3Format) {
  // GREEN PHASE: Now testing actual implementation

  std::string filename = "test.tpx3";
  size_t num_packets = 1000;
  createTPX3TestFile(filename, num_packets);

  std::string filepath = (test_dir_ / filename).string();

  auto mapped_file = MappedFile::open(filepath);

  // Check TPX3 header
  const uint32_t* header =
      reinterpret_cast<const uint32_t*>(mapped_file->data());
  EXPECT_EQ(*header, 0x33585054);  // "TPX3" magic

  // Check we can access as 64-bit packets
  const uint64_t* packets =
      reinterpret_cast<const uint64_t*>(mapped_file->data() + sizeof(uint32_t));
  EXPECT_EQ(packets[0], 0);
  EXPECT_EQ(packets[1], 1);
}

// Test 6: MappedFile should handle empty files
TEST_F(TDCIOTest, HandlesEmptyFiles) {
  // GREEN PHASE: Now testing actual implementation

  std::string filename = "empty.bin";
  createTestFile(filename, 0);

  std::string filepath = (test_dir_ / filename).string();

  // Should handle empty files gracefully
  auto mapped_file = MappedFile::open(filepath);
  EXPECT_EQ(mapped_file->size(), 0);
  EXPECT_EQ(mapped_file->data(), nullptr);  // null for empty files
}

}  // namespace tdcsophiread