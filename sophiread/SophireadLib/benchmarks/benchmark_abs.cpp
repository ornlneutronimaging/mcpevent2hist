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

int main(int argc, char **argv) {
  // generate random data
  int n = 1000000;
  vector<double> data(n);
  random_device rd;
  mt19937 gen(rd());
  uniform_real_distribution<> dis(0, 1);
  for (int i = 0; i < n; i++) {
    data[i] = dis(gen);
  }

  // benchmark
  int niter = 100;
  vector<double> result(n);
  auto start = chrono::high_resolution_clock::now();
  for (int i = 0; i < niter; i++) {
    //
  }
  auto end = chrono::high_resolution_clock::now();
  auto duration =
      chrono::duration_cast<chrono::microseconds>(end - start).count();
  cout << "abs: " << duration / niter << " us" << endl;

  return 0;
}