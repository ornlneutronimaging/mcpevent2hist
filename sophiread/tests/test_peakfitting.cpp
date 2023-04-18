#include <gtest/gtest.h>

#include <iostream>
#include <random>

#include "centroid.h"
#include "fastgaussian.h"

std::random_device rd;
std::mt19937 gen(rd());
std::normal_distribution<> pos(0, 2);
std::normal_distribution<> tot(1000, 2);
std::normal_distribution<> toa(1000, 2);
std::normal_distribution<> ftoa(16, 8);
std::normal_distribution<> tof(0, 2);
std::normal_distribution<> spidertime(0, 10);

TEST(PeakFitting, CentroidAlgorithm) {
  // set tolerance for the absolute position error to half a pixel
  const double absolute_error = 0.01 * DSCALE;
  // Create a random vector of hits
  // NOTE: random number generator do not generate the same number across
  // multiple platforms.
  std::vector<Hit> hits;
  hits.push_back(Hit(1750, 2038, 2445, 1428, 3989, 3026, 740));
  hits.push_back(Hit(3015, 2073, 3212, 718, 2842, 428, 422));
  hits.push_back(Hit(772, 3912, 3133, 2664, 236, 3334, 3134));

  // Create a centroid algorithm
  // CASE_1: weighted by tot
  Centroid alg;
  NeutronEvent event = alg.fit(hits);
  event.toString();

  // Check that the event is correct
  EXPECT_NEAR(event.getX(), 1863.66 * DSCALE, absolute_error)
      << "Centroid x is not correct.";
  EXPECT_NEAR(event.getY(), 2718.74 * DSCALE, absolute_error)
      << "Centroid y is not correct.";
  EXPECT_NEAR(event.getTOF(), 2262.67, absolute_error)
      << "Centroid tof is not correct.";
  EXPECT_NEAR(event.getTOT(),  8790 , absolute_error)
     << "Centroid tot is not correct.";
  EXPECT_NEAR(event.getTOA(), 1603.33 , absolute_error)
     << "Centroid toa is not correct.";

  // CASE_2: not weighted by tot
  Centroid alg2(false);
  NeutronEvent event2 = alg2.fit(hits);

  // Check that the event is correct
  EXPECT_NEAR(event2.getX(), 1845.67 * DSCALE, absolute_error)
      << "Centroid x is not correct.";
  EXPECT_NEAR(event2.getY(), 2674.33 * DSCALE, absolute_error)
      << "Centroid y is not correct.";
  EXPECT_NEAR(event2.getTOF(), 2262.67, absolute_error)
      << "Centroid tof is not correct.";
  EXPECT_NEAR(event.getTOT(), 8790 , absolute_error)
     << "Centroid tot is not correct.";
  EXPECT_NEAR(event.getTOA(), 1603.33 , absolute_error)
     << "Centroid toa is not correct.";
}

TEST(PeakFitting, FastGaussianAlgorithm) {
  // set tolerance for the absolute position error to half a pixel
  const double absolute_error = 0.01 * DSCALE;

  // // Create a cluster of 300 hits
  // std::vector<Hit> hits;
  // for (int i = 0; i < 300; i++) {
  //   int x = 50 + pos(gen);
  //   int y = 50 + pos(gen);
  //   int mytof = 1000 + tof(gen);
  //   int stime = 10 + spidertime(gen);
  //   int mytoa = 10 + toa(gen);
  //   double mytot = 10 + 0.001 * tot(gen);
  //   hits.push_back(
  //       Hit(x, y, (int) mytot, mytoa , ftoa(gen), mytof, stime));
  // }

  // Create a random vector of hits
  // NOTE: random number generator do not generate the same number across
  // multiple platforms.
  std::vector<Hit> hits;
  hits.push_back(Hit(1750, 2038, 2445, 1428, 3989, 3026, 740));
  hits.push_back(Hit(3015, 2073, 3212, 718, 2842, 428, 422));
  hits.push_back(Hit(772, 3912, 3133, 2664, 236, 3334, 3134));

  hits.push_back(Hit(20, 23, 40, 18, 15, 70, 100));
  hits.push_back(Hit(315, 373, 312, 71, 242, 48, 42));
  hits.push_back(Hit(72, 312, 313, 264, 36, 334, 134));

  hits.push_back(Hit(1740, 2238, 2435, 1928, 3489, 3676, 767));
  hits.push_back(Hit(3055, 2973, 3242, 818, 2232, 4678, 465));
  hits.push_back(Hit(777, 3812, 3135, 7664, 232, 6734, 3145));

  hits.push_back(Hit(123, 238, 345, 128, 389, 326, 120));
  hits.push_back(Hit(345, 203, 342, 128, 242, 428, 122));
  hits.push_back(Hit(752, 342, 3154, 2844, 276, 394, 149));

  hits.push_back(Hit(150, 2638, 3445, 3428, 5689, 3026, 740));
  hits.push_back(Hit(415, 2573, 7812, 568, 2676, 489, 122));
  hits.push_back(Hit(372, 4312, 6533, 5464, 252, 3234, 234));


  // Create a fast gaussian algorithm
  FastGaussian alg;
  NeutronEvent event = alg.fit(hits);

  // Check that the event is correct
  EXPECT_NEAR(event.getX(), 1419.00 * DSCALE, absolute_error)
      << "FastGaussian x is not correct.";
  EXPECT_NEAR(event.getY(),  2347.80 * DSCALE, absolute_error)
      << "FastGaussian y is not correct.";
  EXPECT_NEAR(event.getTOF(),  2711.85, absolute_error)
      << "FastGaussian tof is not correct.";
  EXPECT_NEAR(event.getTOT(),  8602, absolute_error)
     << "FastGaussian tot is not correct.";
  EXPECT_NEAR(event.getTOA(),   3072, absolute_error)
     << "FastGaussian toa is not correct.";
}
