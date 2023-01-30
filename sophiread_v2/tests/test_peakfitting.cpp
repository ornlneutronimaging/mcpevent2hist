#include <gtest/gtest.h>

#include <iostream>

#include "centroid.h"
#include "fastgaussian.h"

const double absolute_error = 0.01;

TEST(PeakFitting, CentroidAlgorithm) {
  // Create a random vector of hits
  // NOTE: random number generator do not generate the same number across
  // multiple platforms.
  std::vector<Hit> hits;
  hits.push_back(Hit(1750, 2038, 2445, 1428, 3989, 3026, 740));
  hits.push_back(Hit(3015, 2073, 3212, 718, 2842, 428, 422));
  hits.push_back(Hit(772, 3912, 3133, 2664, 236, 3334, 3134));

  // Create a centroid algorithm
  // CASE_1: weighted by tot
  Centroid centroid;
  NeutronEvent event = centroid.fit(hits);

  // Check that the event is correct
  EXPECT_NEAR(event.getX(), 1863.66, absolute_error)
      << "Centroid x is not correct.";
  EXPECT_NEAR(event.getY(), 2718.74, absolute_error)
      << "Centroid y is not correct.";
  EXPECT_NEAR(event.getTOF(), 2262.67, absolute_error)
      << "Centroid tof is not correct.";

  // CASE_2: not weighted by tot
  Centroid centroid2(false);
  NeutronEvent event2 = centroid2.fit(hits);

  // Check that the event is correct
  EXPECT_NEAR(event2.getX(), 1845.67, absolute_error)
      << "Centroid x is not correct.";
  EXPECT_NEAR(event2.getY(), 2674.33, absolute_error)
      << "Centroid y is not correct.";
  EXPECT_NEAR(event2.getTOF(), 2262.67, absolute_error)
      << "Centroid tof is not correct.";
}

TEST(PeakFitting, FastGaussianAlgorithm) {
  std::cout << "FastGaussian to be implemented." << std::endl;
}
