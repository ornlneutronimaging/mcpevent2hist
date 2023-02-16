#pragma once

#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

// scale factor used for super-pixeling (1 pixel -> 8x8 pixels)
#define DSCALE 8.0

/**
 * @brief Class to store the data of a single hit of a charged particle
 *        hitting the timepix3 sensor.
 *
 */
class Hit {
 public:
  // copy constructor
  Hit(const Hit& hit)
      : m_x(hit.m_x),
        m_y(hit.m_y),
        m_tot(hit.m_tot),
        m_toa(hit.m_toa),
        m_ftoa(hit.m_ftoa),
        m_tof(hit.m_tof),
        m_spidertime(hit.m_spidertime){};

  Hit(int x, int y, int tot, int toa, int ftoa, unsigned int tof,
      unsigned long long spidertime)
      : m_x(x),
        m_y(y),
        m_tot(tot),
        m_toa(toa),
        m_ftoa(ftoa),
        m_tof(tof),
        m_spidertime(spidertime){};

  Hit& operator=(const Hit& hit) {
    m_x = hit.m_x;
    m_y = hit.m_y;
    m_tot = hit.m_tot;
    m_toa = hit.m_toa;
    m_ftoa = hit.m_ftoa;
    m_tof = hit.m_tof;
    m_spidertime = hit.m_spidertime;
    return *this;
  }

  int getX() const { return m_x; };
  int getY() const { return m_y; };
  int getTOT() const { return m_tot; };
  int getTOA() const { return m_toa; };
  int getFTOA() const { return m_ftoa; };
  unsigned long long getSPIDERTIME() const { return m_spidertime; };
  unsigned int getTOF() const { return m_tof; };

  double getTOF_ns() const { return m_tof * m_scale_to_ns_40mhz; };
  double getTOA_ns() const { return m_toa * m_scale_to_ns_40mhz; };
  double getTOT_ns() const { return m_tot * m_scale_to_ns_40mhz; };
  double getSPIDERTIME_ns() const {
    return m_spidertime * m_scale_to_ns_40mhz;
  };
  double getFTOA_ns() const { return m_ftoa * m_scale_to_ns_640mhz; };

  std::string toString() const;

 private:
  // raw packet directly read from tpx3.
  int m_x, m_y;  // pixel coordinates
  int m_tot;     // time over threshold
  int m_toa;     // time of arrival (40MHz clock, 14 bit)
  int m_ftoa;    // fine time of arrival (640MHz clock, 4 bit)
  unsigned int m_tof;
  unsigned long long m_spidertime;  // time from the spider board (in the unit of 25ns)

  // scale factor that converts time to ns
  const double m_scale_to_ns_40mhz =
      25.0;  // 40 MHz clock is used for the coarse time of arrival.
  const double m_scale_to_ns_640mhz =
      25.0 / 16.0;  // 640 MHz clock is used for the fine time of arrival.
};

/**
 * @brief Class to store the data of a single neutron event.
 *
 */
class NeutronEvent {
 public:
  NeutronEvent(const double x, const double y, const double tof,
               const int nHits)
      : m_x(x), m_y(y), m_tof(tof), m_nHits(nHits){};
  double getX() const { return m_x; };
  double getY() const { return m_y; };
  double getTOF() const { return m_tof; };
  double getTOF_ns() const { return m_tof * m_scale_to_ns_40mhz; };
  int getNHits() const { return m_nHits; };

  std::string toString() const;

 private:
  const double m_x, m_y;  // pixel coordinates
  const double m_tof;     // time of flight
  const int m_nHits;      // number of hits in the event (cluster size)
  const double m_scale_to_ns_40mhz =
      25.0;  // 40 MHz clock is used for the coarse time of arrival.
};

// static file processing
std::vector<Hit> readTimepix3RawData(const std::string& filepath);
Hit packetToHit(const std::vector<char>& packet, const unsigned long long tdc,
                const unsigned long long gdc, const int chip_layout_type);
//
void saveHitsToHDF5(const std::string out_file_name,
                    const std::vector<Hit>& hits,
                    const std::vector<int>& labels);
void saveEventsToHDF5(const std::string out_file_name,
                      const std::vector<NeutronEvent>& events);
