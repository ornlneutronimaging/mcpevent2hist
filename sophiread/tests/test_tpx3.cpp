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

TEST(FileHandlingTest, VerifyRollover){
  // reading the testing raw data 
  auto hits =
      readTimepix3RawData("data/rollover_test_data.tpx3");

  // check the number of hits
  EXPECT_EQ(hits.size(),26);

  // check disordered pixels: increment counter 
  EXPECT_EQ(hits[0].getSPIDERTIME(),1006649343);               // 25.1662, 1006649343
  EXPECT_EQ(hits[1].getSPIDERTIME(),1069563903);               // 26.7391, 1069563903
  EXPECT_EQ(hits[2].getSPIDERTIME(),1073741823);               // 26.8435, 1073741823
  EXPECT_EQ(hits[3].getSPIDERTIME(),262143 + 1073741824);      // 0.00655357, 262143
  EXPECT_EQ(hits[4].getSPIDERTIME(),4194303 + 1073741824);     // 0.104858, 4194303
  EXPECT_EQ(hits[5].getSPIDERTIME(),67108863 + 1073741824);    // 1.67772, 67108863
  EXPECT_EQ(hits[6].getSPIDERTIME(), 201326591 + 1073741824);  // 5.03316, 201326591

  // check disordered pixels: do nothing to counter
  EXPECT_EQ(hits[7].getSPIDERTIME(),262143 + 1073741824);      // 0.00655357, 262143
  EXPECT_EQ(hits[8].getSPIDERTIME(),268435455 + 1073741824);   // 6.71089, 268435455
  EXPECT_EQ(hits[9].getSPIDERTIME(),4194303 + 1073741824);     // 0.104858, 4194303   
  EXPECT_EQ(hits[10].getSPIDERTIME(),67108863 + 1073741824);   // 1.67772, 67108863
  EXPECT_EQ(hits[11].getSPIDERTIME(),134217727 + 1073741824);  // 3.35544, 134217727
  EXPECT_EQ(hits[12].getSPIDERTIME(),201326591 + 1073741824);  // 5.03316, 201326591

  // check order pixels: decrement counter 
  EXPECT_EQ(hits[13].getSPIDERTIME(),262143 + 1073741824);         // 0.00655357, 262143
  EXPECT_EQ(hits[14].getSPIDERTIME(),4194303 + 1073741824);        // 0.104858, 4194303
  EXPECT_EQ(hits[15].getSPIDERTIME(),67108863 + 1073741824);       // 1.67772, 67108863
  EXPECT_EQ(hits[16].getSPIDERTIME(),201326591 + 1073741824);      // 5.03316, 201326591
  EXPECT_EQ(hits[17].getSPIDERTIME(),1006649343);     // 25.1662, 1006649343
  EXPECT_EQ(hits[18].getSPIDERTIME(),1069563903);     // 26.7391, 1069563903
  EXPECT_EQ(hits[19].getSPIDERTIME(),1073741823);     // 26.8435, 1073741823

  // check ordered pixels: do nothing to counter 
  EXPECT_EQ(hits[20].getSPIDERTIME(),262143 + 1073741824);     // 0.00655357, 262143
  EXPECT_EQ(hits[21].getSPIDERTIME(),4194303 + 1073741824);    // 0.104858, 4194303
  EXPECT_EQ(hits[22].getSPIDERTIME(),67108863 + 1073741824);   // 1.67772, 67108863
  EXPECT_EQ(hits[23].getSPIDERTIME(),134217727 + 1073741824);  // 3.35544, 134217727
  EXPECT_EQ(hits[24].getSPIDERTIME(),201326591 + 1073741824);  // 5.03316, 201326591
  EXPECT_EQ(hits[25].getSPIDERTIME(),268435455 + 1073741824);  // 6.71089, 268435455

}

// Test the parseUserDefinedParams function
TEST(FileHandlingTest, ParseUserDefinedParams) {

  // read user-defined param files 
  auto p1 = parseUserDefinedParams("../user_defined_params.txt");
  auto p2 = parseUserDefinedParams("../user_defined_params_1.txt");

  // check user-defined params 
  EXPECT_EQ(p1.getABSRadius(), 5.0);
  EXPECT_EQ(p1.getABSMinClusterSize(),1);
  EXPECT_EQ(p1.getABSSpidertimeRange(), 75);

  EXPECT_EQ(p2.getABSRadius(), 20.0);
  EXPECT_EQ(p2.getABSMinClusterSize(),30);
  EXPECT_EQ(p2.getABSSpidertimeRange(),500000);

}