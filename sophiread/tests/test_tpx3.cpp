#include <gtest/gtest.h>
#include <iostream>

#include "tpx3.h"

// Test the readTimepix3RawData function
TEST(FileHandlingTest, ReadTPX3RawData) {
  // read the testing raw data
  auto hits =
      readTimepix3RawData("data/frames_pinhole_3mm_1s_RESOLUTION_000001.tpx3");

  std::cout << hits[0].getX() << " " 
            << hits[0].getY() << " " 
            << hits[0].getTOT() << " " 
            << hits[0].getTOA() << " " 
            << hits[0].getFTOA() << " " 
            << hits[0].getTOF() << " " 
            << hits[0].getSPIDERTIME() << std::endl;

  std::cout << hits[9933804 - 1].getX() << " "  
            << hits[9933804 - 1].getY() << " " 
            << hits[9933804 - 1].getTOT() << " " 
            << hits[9933804 - 1].getTOA() << " " 
            << hits[9933804 - 1].getFTOA() << " " 
            << hits[9933804 - 1].getTOF() << " " 
            << hits[9933804 - 1].getSPIDERTIME() << std::endl;
            
  // check the number of hits
  EXPECT_EQ(hits.size(), 9933804);
  // // check the first hit
  EXPECT_EQ(hits[0].getX(), 49);
  EXPECT_EQ(hits[0].getY(), 380);
  EXPECT_EQ(hits[0].getTOT(), 2);
  EXPECT_EQ(hits[0].getTOA(), 6769);
  EXPECT_EQ(hits[0].getFTOA(), 15);
  EXPECT_EQ(hits[0].getTOF(), 3234077297);
  EXPECT_EQ(hits[0].getSPIDERTIME(), 12851825);
  // check the last hit
  EXPECT_EQ(hits[9933804 - 1].getX(), 462);
  EXPECT_EQ(hits[9933804 - 1].getY(), 448);
  EXPECT_EQ(hits[9933804 - 1].getTOT(), 16);
  EXPECT_EQ(hits[9933804 - 1].getTOA(), 11297);
  EXPECT_EQ(hits[9933804 - 1].getFTOA(), 9);
  EXPECT_EQ(hits[9933804 - 1].getTOF(), 3274075169);
  EXPECT_EQ(hits[9933804 - 1].getSPIDERTIME(), 52849697);



}
