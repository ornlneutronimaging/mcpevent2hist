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
      const auto hit_x = hit.getX();
      const auto hit_y = hit.getY();
      const auto hit_tot = hit.getTOT();
      const auto hit_tof = hit.getTOF();

      x += DSCALE * hit_x * hit_tot;
      y += DSCALE * hit_y * hit_tot;
      tof += hit_tof;
      tot += hit_tot;
    }
    const auto tot_inv = 1.0 / tot;
    x *= tot_inv;
    y *= tot_inv;
  } else {
    for (const auto& hit : data) {
      x += DSCALE * hit.getX();
      y += DSCALE * hit.getY();
      tof += hit.getTOF();
      tot += hit.getTOT();
    }
    const auto data_size_inv = 1.0 / data.size();
    x *= data_size_inv;
    y *= data_size_inv;
  }

  tof /= data.size();

  return NeutronEvent(x, y, tof, tot, data.size());
}
