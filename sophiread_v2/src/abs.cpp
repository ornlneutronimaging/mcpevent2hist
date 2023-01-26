#include "abs.h"

#include <iostream>

/**
 * @brief Fit the ARS algorithm to the data
 *
 * @param data
 */
void ABS::fit(const std::vector<Hit>& data) {
  std::cout << "ARS to be implemented." << std::endl;
}

/**
 * @brief Predict the clusters by retrieving the labels of the hits
 *
 * @param data
 * @return std::vector<int>
 */
std::vector<NeutronEvent> ABS::get_events(const std::vector<Hit>& data) {
  std::cout << "ARS to be implemented." << std::endl;
  return std::vector<NeutronEvent>();
}
