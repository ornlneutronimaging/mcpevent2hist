#include <gtest/gtest.h>
#include "tpx3.h"

// Test the readTimepix3RawData function
TEST(FileHandlingTest, ReadTPX3RawData) {
  // read the testing raw data
  auto hits = readTimepix3RawData("data/frames_pinhole_3mm_1s_RESOLUTION_000001.tpx3");
  // check the number of hits
  EXPECT_EQ(hits.size(), 9933804);
  // // check the first hit
  EXPECT_EQ(hits[0].getX(), 49);
  EXPECT_EQ(hits[0].getY(), 380);
  EXPECT_EQ(hits[0].getTOT(), 2);
  EXPECT_EQ(hits[0].getTOA(), 6769);
  EXPECT_EQ(hits[0].getFTOA(), 15);
  EXPECT_EQ(hits[0].getTOF(), 12851825);
  EXPECT_EQ(hits[0].getSPIDERTIME(), 12851825);
  // check the last hit
  EXPECT_EQ(hits[9933804 - 1].getX(), 462);
  EXPECT_EQ(hits[9933804 - 1].getY(), 448);
  EXPECT_EQ(hits[9933804 - 1].getTOT(), 16);
  EXPECT_EQ(hits[9933804 - 1].getTOA(), 11297);
  EXPECT_EQ(hits[9933804 - 1].getFTOA(), 9);
  EXPECT_EQ(hits[9933804 - 1].getTOF(), 52849697);
  EXPECT_EQ(hits[9933804 - 1].getSPIDERTIME(), 52849697);
}
