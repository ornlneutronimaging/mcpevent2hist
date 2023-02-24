#include <gtest/gtest.h>
#include <iostream>

#include "tpx3.h"

// Test the readTimepix3RawData function
TEST(FileHandlingTest, ReadTPX3RawData) {
  // read the testing raw data
  auto hits =
      readTimepix3RawData("data/frames_pinhole_3mm_1s_RESOLUTION_000001.tpx3");

  // check the number of hits
  EXPECT_EQ(hits.size(), 9933804);
  // // check the first hit
  EXPECT_EQ(hits[0].getX(), 49);
  EXPECT_EQ(hits[0].getY(), 380);
  EXPECT_EQ(hits[0].getTOT(), 2);
  EXPECT_EQ(hits[0].getTOA(), 6769);
  EXPECT_EQ(hits[0].getFTOA(), 15);
  // EXPECT_EQ(hits[0].getTOF(), 3234077297);
  // EXPECT_EQ(hits[0].getSPIDERTIME(), 3234077297);
  // check the last hit
  EXPECT_EQ(hits[9933804 - 1].getX(), 462);
  EXPECT_EQ(hits[9933804 - 1].getY(), 448);
  EXPECT_EQ(hits[9933804 - 1].getTOT(), 16);
  EXPECT_EQ(hits[9933804 - 1].getTOA(), 11297);
  EXPECT_EQ(hits[9933804 - 1].getFTOA(), 9);
  // EXPECT_EQ(hits[9933804 - 1].getTOF(), 3274075169);
  // EXPECT_EQ(hits[9933804 - 1].getSPIDERTIME(), 3274075169);
}

TEST(FileHandlingTest, VerifyTiming) {
  // read the testing raw data
  auto hits =
      readTimepix3RawData("data/greg_220ms_TDC_GDC_enabled_20230214_3.tpx3");

  // check the number of hits
  EXPECT_EQ(hits.size(), 365);
  // // check the first hit
  EXPECT_EQ(hits[0].getX(), 146);
  EXPECT_EQ(hits[0].getY(), 310);
  EXPECT_EQ(hits[0].getTOT(), 2);
  EXPECT_EQ(hits[0].getTOA(), 1293);
  EXPECT_EQ(hits[0].getFTOA(), 4);
  // EXPECT_EQ(hits[0].getTOF(), 414131);
  // EXPECT_EQ(hits[0].getSPIDERTIME(), 2443848844557);
  // check the last hit
  EXPECT_EQ(hits[365 - 1].getX(), 104);
  EXPECT_EQ(hits[365 - 1].getY(), 415);
  EXPECT_EQ(hits[365 - 1].getTOT(), 43);
  EXPECT_EQ(hits[365 - 1].getTOA(), 14683);
  EXPECT_EQ(hits[365 - 1].getFTOA(), 11);
  // EXPECT_EQ(hits[365 - 1].getTOF(), 172774);
  // EXPECT_EQ(hits[365 - 1].getSPIDERTIME(), 8809347419);
}