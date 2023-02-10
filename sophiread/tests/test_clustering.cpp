#include <gtest/gtest.h>

#include <algorithm>
#include <iostream>
#include <random>

#include "abs.h"
#include "dbscan.h"

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<> pos(0, 2);
std::uniform_real_distribution<> tot(0, 100);
std::uniform_real_distribution<> toa(0, 1000);
std::uniform_real_distribution<> ftoa(0, 255);
std::uniform_real_distribution<> tof(0, 2000);
std::uniform_real_distribution<> spidertime(-1, 1);

std::vector<Hit> gen_clusters() {
  // create 3 clusters of 20 hits each
  std::vector<Hit> data;
  // cluster 1 around (50, 50)
  for (int i = 0; i < 10; i++) {
    int x = 50 + pos(gen);
    int y = 50 + pos(gen);
    int stime = 10 + spidertime(gen);
    data.push_back(Hit(x, y, tot(gen), toa(gen), ftoa(gen), tof(gen), stime));
  }
  // cluster 2 around (100, 100)
  for (int i = 0; i < 10; i++) {
    int x = 100 + pos(gen);
    int y = 100 + pos(gen);
    int stime = 15 + spidertime(gen);
    data.push_back(Hit(x, y, tot(gen), toa(gen), ftoa(gen), tof(gen), stime));
  }
  // cluster 3 around (150, 150)
  for (int i = 0; i < 10; i++) {
    int x = 150 + pos(gen);
    int y = 150 + pos(gen);
    int stime = 20 + spidertime(gen);
    data.push_back(Hit(x, y, tot(gen), toa(gen), ftoa(gen), tof(gen), stime));
  }
  return data;
}

TEST(Clustering, ABSAlgorithm) {
  // set tolerance for the absolute position error to half a pixel
  // NOTE: adaptive box search is not a robust peak fitting method, therefore we
  //       need to set a large tolerance for the absolute position error
  const double absolute_pos_error = 2.0 * DSCALE;

  // create 3 clusters of 20 hits each
  auto data = gen_clusters();

  // create the ABS algorithm
  ABS abs(5);
  abs.fit(data);
  abs.set_method("centroid");
  auto events = abs.get_events(data);

  // check that there are 3 events
  EXPECT_EQ(events.size(), 3);

  // OPENMP will shuffle the order of the events, we need to extract and sort
  // them before asserting
  std::vector<double> x = {events[0].getX(), events[1].getX(),
                           events[2].getX()};
  std::vector<double> y = {events[0].getY(), events[1].getY(),
                           events[2].getY()};
  std::sort(x.begin(), x.end());
  std::sort(y.begin(), y.end());

  // check the events x, y coordinates
  EXPECT_NEAR(x[0], 50 * DSCALE, absolute_pos_error);
  EXPECT_NEAR(y[0], 50 * DSCALE, absolute_pos_error);
  EXPECT_NEAR(x[1], 100 * DSCALE, absolute_pos_error);
  EXPECT_NEAR(y[1], 100 * DSCALE, absolute_pos_error);
  EXPECT_NEAR(x[2], 150 * DSCALE, absolute_pos_error);
  EXPECT_NEAR(y[2], 150 * DSCALE, absolute_pos_error);

  // print out the events
  std::cout << "Events:" << std::endl;
  for (auto& event : events) {
    std::cout << event.toString() << std::endl;
  }
}

TEST(Clustering, DBSCANAlgorithm) {
  const double absolute_pos_error = 2.0;
  
  auto data = gen_clusters();
  // create the DSCAN algorithm
  DBSCAN dbs(8000./*eps time*/, 30/*min_points time*/, 4./*eps xy*/, 10/*min_points xy*/);  
  auto events = dbs.get_events(data);
  //dbs.fit(data);
  
  // check that there are 3 events
  EXPECT_EQ(events.size(), 3);

  std::vector<double> x = {events[0].getX(), events[1].getX(),
                           events[2].getX()};
  std::vector<double> y = {events[0].getY(), events[1].getY(),
                           events[2].getY()};
  std::sort(x.begin(), x.end());
  std::sort(y.begin(), y.end());

  // check the events x, y coordinates
  EXPECT_NEAR(x[0], 50, absolute_pos_error);
  EXPECT_NEAR(y[0], 50, absolute_pos_error);
  EXPECT_NEAR(x[1], 100, absolute_pos_error);
  EXPECT_NEAR(y[1], 100, absolute_pos_error);
  EXPECT_NEAR(x[2], 150, absolute_pos_error);
  EXPECT_NEAR(y[2], 150, absolute_pos_error);

  // print out the events
  std::cout << "Events:" << std::endl;
  for (auto& event : events) {
    std::cout << event.toString() << std::endl;
  }
}
