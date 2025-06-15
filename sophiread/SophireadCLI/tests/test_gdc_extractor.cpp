#include <gtest/gtest.h>

#include "gdc_extractor.h"

TEST(GDCExtractorOptionsTest, ChunkSizeValidation) {
  // Test minimum chunk size validation
  sophiread::GDCExtractorOptions min_opts;
  min_opts.input_tpx3 = "/dev/null";
  min_opts.output_csv = "test.csv";
  min_opts.chunk_size = min_opts.MIN_CHUNK_SIZE - 1;  // Just below minimum
  EXPECT_FALSE(min_opts.validate());

  // Test maximum chunk size validation
  sophiread::GDCExtractorOptions max_opts;
  max_opts.input_tpx3 = "/dev/null";
  max_opts.output_csv = "test.csv";
  max_opts.chunk_size = max_opts.MAX_CHUNK_SIZE + 1;  // Just above maximum
  EXPECT_FALSE(max_opts.validate());

  // Test valid chunk size
  sophiread::GDCExtractorOptions valid_opts;
  valid_opts.input_tpx3 = "/dev/null";
  valid_opts.output_csv = "test.csv";
  valid_opts.chunk_size = 10ULL * 1024 * 1024;  // 10MB
  EXPECT_TRUE(valid_opts.validate());
}

TEST(GDCExtractorOptionsTest, FileAccessValidation) {
  sophiread::GDCExtractorOptions opts;
  opts.chunk_size = 10ULL * 1024 * 1024;  // Valid chunk size

  // Test non-existent input file
  opts.input_tpx3 = "nonexistent.tpx3";
  EXPECT_FALSE(opts.validate());

  // Test non-writable output path
  opts.input_tpx3 = "/dev/null";  // Exists and readable
  opts.output_csv = "/nonexistent/path/output.csv";
  EXPECT_FALSE(opts.validate());
}