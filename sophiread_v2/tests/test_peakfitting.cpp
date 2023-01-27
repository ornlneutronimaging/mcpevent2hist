#include <gtest/gtest.h>

#include <iostream>
#include <random>

#include "centroid.h"
#include "fastgaussian.h"

std::mt19937 random_engine(1960);  // seed the random engine
std::uniform_int_distribution<int> distribution(0, 4096);  // 4k detector
const double absolute_error = 0.01;

Hit generateRandomHit() {
  int x = distribution(random_engine);
  int y = distribution(random_engine);
  int tot = distribution(random_engine);
  int toa = distribution(random_engine);
  int ftoa = distribution(random_engine);
  int tof = distribution(random_engine);
  int spidertime = distribution(random_engine);
  return Hit(x, y, tot, toa, ftoa, tof, spidertime);
}

TEST(PeakFitting, CentroidAlgorithm) {
  // Create a random vector of hits
  std::vector<Hit> hits;
  for (int i = 0; i < 3; i++) {
    hits.push_back(generateRandomHit());
  }

  // print out the hits
  for (const auto& hit : hits) {
    std::cout << hit.toString() << std::endl;
  }

  // Create a centroid algorithm
  Centroid centroid;
  NeutronEvent event = centroid.fit(hits);

  // Check that the event is correct
  EXPECT_NEAR(event.getX(), 1863.66, absolute_error)
      << "Centroid x is not correct.";
  EXPECT_NEAR(event.getY(), 2718.74, absolute_error)
      << "Centroid y is not correct.";
  EXPECT_NEAR(event.getTOF(), 2262.67, absolute_error)
      << "Centroid tof is not correct.";
}

TEST(PeakFitting, FastGaussianAlgorithm) {
  std::cout << "FastGaussian to be implemented." << std::endl;
}
