#include "ars.h"

#include <iostream>

void ARS::initialize(std::string method) {
  std::cout << "ARS to be implemented." << std::endl;
}

/**
 * @brief Fit the ARS algorithm to the data
 *
 * @param data
 */
void ARS::fit(const std::vector<Hit>& data) {
  std::cout << "ARS to be implemented." << std::endl;
}

/**
 * @brief Predict the clusters by retrieving the labels of the hits
 *
 * @param data
 * @return std::vector<int>
 */
std::vector<NeutronEvent> ARS::predict(const std::vector<Hit>& data) {
  std::cout << "ARS to be implemented." << std::endl;
  return std::vector<NeutronEvent>();
}
