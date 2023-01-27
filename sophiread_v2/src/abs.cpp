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
  std::cout << "ARS to be implemented, using dummy events now" << std::endl;
  std::vector<NeutronEvent> events;
  for (int i = 0; i < 100; i++) {
    events.push_back(NeutronEvent(50, 80, 100, 30));
  }
  return events;
}
