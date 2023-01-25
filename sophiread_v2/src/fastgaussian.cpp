#include "fastgaussian.h"

#include <iostream>

void FastGaussian::initialize() {
  std::cout << "FastGaussian to be implemented." << std::endl;
}

void FastGaussian::fit(const std::vector<Hit>& data) {
  std::cout << "FastGaussian to be implemented." << std::endl;
}

std::vector<std::tuple<double, double, double>> FastGaussian::predict(
    const std::vector<Hit>& data) {
  std::vector<std::tuple<double, double, double>> predictions = {};
  std::cout << "FastGaussian to be implemented." << std::endl;
  return predictions;
}
