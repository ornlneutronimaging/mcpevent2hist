#include "centroid.h"

#include <iostream>

void Centroid::initialize() {
  std::cout << "Centroid to be implemented." << std::endl;
}

void Centroid::fit(const std::vector<Hit>& data) {
  std::cout << "Centroid to be implemented." << std::endl;
}

std::vector<std::tuple<double, double, double>> Centroid::predict(
    const std::vector<Hit>& data) {
  std::vector<std::tuple<double, double, double>> predictions = {};
  std::cout << "Centroid to be implemented." << std::endl;
  return predictions;
}
