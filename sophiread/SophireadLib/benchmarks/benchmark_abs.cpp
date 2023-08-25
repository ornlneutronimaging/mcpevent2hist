/**
 * @file abs.cpp
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief Benchmark the performance of abs clustering method
 * @version 0.1
 * @date 2023-08-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

#include "abs.h"

using namespace std;
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<> pos(0, 2);
std::uniform_real_distribution<> tot(0, 100);
std::uniform_real_distribution<> toa(0, 1000);
std::uniform_real_distribution<> ftoa(0, 255);
std::uniform_real_distribution<> tof(0, 2000);
std::uniform_real_distribution<> spidertime(-1, 1);

/*
Target processing speed: 120,000,000 hits / sec -> 120 hits/us
  -> 12 clusters, 10 hits each == 120 hits -> 1 us
  -> 12000 clusters, 10 hits each == 120000 hits -> 1000 us
*/

std::vector<Hit> fake_hits() {
  std::vector<Hit> hits;
  // generate 12000 clusters of 10 hits each
  for (int i = 0; i < 12000; i++) {
    // cluster center
    int x = 10 * i + pos(gen);
    int y = 10 * i + pos(gen);
    int stime = 10 * i + spidertime(gen);
    // cluster
    for (int j = 0; j < 10; j++) {
      hits.push_back(Hit(x, y, tot(gen), toa(gen), ftoa(gen), tof(gen), stime));
    }
  }
  return hits;
}

double run_single_test(int run_id, std::vector<Hit> hits, double &fit_time,
                       double &events_time) {
  // create ABS algorithm
  ABS abs_alg(5.0, 1, 75);

  // fit hits into clusters
  auto start_fit = chrono::high_resolution_clock::now();
  abs_alg.fit(hits);
  auto end_fit = chrono::high_resolution_clock::now();
  auto duration_fit =
      chrono::duration_cast<chrono::microseconds>(end_fit - start_fit).count();
  cout << "abs::fit " << duration_fit << " us" << endl;

  // convert to neutron events
  auto start_events = chrono::high_resolution_clock::now();
  abs_alg.set_method("centroid");
  auto events = abs_alg.get_events(hits);
  auto end_events = chrono::high_resolution_clock::now();
  auto duration_events =
      chrono::duration_cast<chrono::microseconds>(end_events - start_events)
          .count();
  cout << "abs::get_events " << duration_events << " us" << endl;

  fit_time += duration_fit;
  events_time += duration_events;

  // release memory
  abs_alg.reset();

  return duration_fit + duration_events;
}

int main() {
  // generate fake hits
  auto hits = fake_hits();

  // run 100 tests and get the average
  const int num_tests = 100;
  double total_time = 0;
  double fit_time = 0;
  double events_time = 0;
  for (int i = 0; i < num_tests; i++) {
    total_time += run_single_test(i, hits, fit_time, events_time);
  }
  cout << "For 120,000 hits (ref time cap: 1000 us):" << endl
       << "Average total time: " << total_time / num_tests << " us" << endl
       << "Average fit time: " << fit_time / num_tests << " us" << endl
       << "Average events time: " << events_time / num_tests << " us" << endl;

  return 0;
}