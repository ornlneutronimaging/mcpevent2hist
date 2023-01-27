#include "centroid.h"

#include <iostream>

NeutronEvent Centroid::fit(const std::vector<Hit>& data) {
  // Calculate the average x, and y using tot as weight
  double x = 0;
  double y = 0;
  double tof = 0;
  double tot = 0;
  for (const auto& hit : data) {
    x += hit.getX() * hit.getTOT();
    y += hit.getY() * hit.getTOT();
    tof += hit.getTOF();
    tot += hit.getTOT();
  }
  x /= tot;
  y /= tot;
  tof /= data.size();
  return NeutronEvent(x, y, tof, data.size());
}
