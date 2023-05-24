#include "centroid.h"

#include <iostream>

#include "tpx3.h"

/**
 * @brief Perform centroid fitting on the hits.
 *
 * @param data: a vector of hits.
 * @return NeutronEvent: a neutron event.
 */
NeutronEvent Centroid::fit(const std::vector<Hit>& data) {
  double x = 0;
  double y = 0;
  double tof = 0;
  double tot = 0;

  if (data.size() == 0) {
    return NeutronEvent(0, 0, 0, 0, 0);
  }

  if (weighted_by_tot) {
    for (const auto& hit : data) {
      x += DSCALE * hit.getX() * hit.getTOT();
      y += DSCALE * hit.getY() * hit.getTOT();
      tof += hit.getTOF();
      tot += hit.getTOT();
    }
    x /= tot;
    y /= tot;
  } else {
    for (const auto& hit : data) {
      x += DSCALE * hit.getX();
      y += DSCALE * hit.getY();
      tof += hit.getTOF();
      tot += hit.getTOT();
    }
    x /= data.size();
    y /= data.size();
  }

  tof /= data.size();
  
  return NeutronEvent(x, y, tof, tot, data.size());
}
