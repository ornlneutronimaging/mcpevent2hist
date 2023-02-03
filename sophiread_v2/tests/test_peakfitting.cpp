#include <gtest/gtest.h>

#include <iostream>
#include <random>

#include "centroid.h"
#include "fastgaussian.h"

std::random_device rd;
std::mt19937 gen(rd());
std::normal_distribution<> pos(0, 2);
std::normal_distribution<> tot(20, 5);
std::normal_distribution<> toa(1000, 200);
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

  // Check that the event is correct
  EXPECT_NEAR(event.getX(), 1863.66 * DSCALE, absolute_error)
      << "Centroid x is not correct.";
  EXPECT_NEAR(event.getY(), 2718.74 * DSCALE, absolute_error)
      << "Centroid y is not correct.";
  EXPECT_NEAR(event.getTOF(), 2262.67, absolute_error)
      << "Centroid tof is not correct.";

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
}

TEST(PeakFitting, FastGaussianAlgorithm) {
  // set tolerance for the absolute position error to half a pixel
  const double absolute_error = 1.0 * DSCALE;

  // Create a cluster of 300 hits
  std::vector<Hit> hits;
  for (int i = 0; i < 300; i++) {
    int x = 50 + pos(gen);
    int y = 50 + pos(gen);
    int mytof = 1000 + tof(gen);
    int stime = 10 + spidertime(gen);
    hits.push_back(
        Hit(x, y, 1 + tot(gen), 1 + toa(gen), ftoa(gen), mytof, stime));
  }

  // Create a fast gaussian algorithm
  FastGaussian alg;
  NeutronEvent event = alg.fit(hits);

  // Check that the event is correct
  EXPECT_NEAR(event.getX(), 50 * DSCALE, absolute_error)
      << "FastGaussian x is not correct.";
  EXPECT_NEAR(event.getY(), 50 * DSCALE, absolute_error)
      << "FastGaussian y is not correct.";
  EXPECT_NEAR(event.getTOF(), 1000, absolute_error)
      << "FastGaussian tof is not correct.";
}
