#include "dbscan.h"

#include <iostream>

/**
 * @brief Fit the DBSCAN algorithm to the data
 *
 * @param hits
 */
void DBSCAN::fit(const std::vector<Hit>& hits) {
  std::cout << "DBSCAN to be implemented." << std::endl;
}

/**
 * @brief Predict the clusters by retrieving the labels of the hits
 *
 * @param hits
 * @return std::vector<int>
 */
std::vector<NeutronEvent> DBSCAN::get_events(const std::vector<Hit>& hits) {
  std::cout << "DBSCAN to be implemented." << std::endl;
  return std::vector<NeutronEvent>();
}
