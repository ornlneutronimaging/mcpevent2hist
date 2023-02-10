#include "tpx3.h"

#include <H5Cpp.h>

#include <algorithm>
#include <iostream>
#include <sstream>

#include "abs.h"
#include "dbscan.h"

std::string Hit::toString() const {
  std::stringstream ss;
  ss << "Hit: x=" << m_x << ", y=" << m_y << ", tot=" << m_tot
     << ", toa=" << m_toa << ", ftoa=" << m_ftoa << ", tof=" << m_tof
     << ", spidertime=" << m_spidertime;
  return ss.str();
}

std::string NeutronEvent::toString() const {
  std::stringstream ss;
  ss << "NeutronEvent: x=" << m_x << ", y=" << m_y << ", tof=" << m_tof
     << ", nHits=" << m_nHits;
  return ss.str();
}

/**
 * @brief Convert a raw data packet into a hit.
 *
 * @param packet: raw data packet
 * @param tdc: time stamp collected from the monitor
 * @param chip_layout_type: chip layout ID number
 * @return Hit
 */
Hit packetToHit(const std::vector<char> &packet, const unsigned long tdc,
                const int chip_layout_type) {
  unsigned short pixaddr, dcol, spix, pix;
  unsigned short *spider_time;
  unsigned short *nTOT;    // bytes 2,3, raw time over threshold
  unsigned int *nTOA;      // bytes 3,4,5,6, raw time of arrival
  unsigned int *npixaddr;  // bytes 4,5,6,7
  int x, y, tot, toa, ftoa, spidertime;
  unsigned int tof;
  // timing information
  spider_time = (unsigned short *)(&packet[0]);  // Spider time  (16 bits)
  nTOT = (unsigned short *)(&packet[2]);         // ToT          (10 bits)
  nTOA = (unsigned int *)(&packet[3]);           // ToA          (14 bits)
  ftoa = *nTOT & 0xF;                            // fine ToA     (4 bits)
  tot = (*nTOT >> 4) & 0x3FF;
  toa = (*nTOA >> 6) & 0x3FFF;
  spidertime = 16384 * (*spider_time) + toa;
  tof = spidertime - tdc;
  // pixel address
  npixaddr = (unsigned int *)(&packet[4]);  // Pixel address (14 bits)
  pixaddr = (*npixaddr >> 12) & 0xFFFF;
  dcol = ((pixaddr & 0xFE00) >> 8);
  spix = ((pixaddr & 0x1F8) >> 1);
  pix = pixaddr & 0x7;
  x = dcol + (pix >> 2);   // x coordinate
  y = spix + (pix & 0x3);  // y coordinate
  // adjustment for chip layout
  if (chip_layout_type == 0) {  // single
    x += 260;
    y = y;
  } else if (chip_layout_type == 1) {  // double
    x = 255 - x + 260;
    y = 255 - y + 260;
  } else if (chip_layout_type == 2) {  // triple
    x = 255 - x;
    y = 255 - y + 260;
  } else {  // quad
    x = x;
    y = y;
  }
  // return the hit
  return Hit(x, y, tot, toa, ftoa, tof, spidertime);
}

/**
 * @brief Read the raw data from a TimePix3 file.
 *
 * @param filepath: path to the raw data file.
 * @return std::vector<Hit>: vector of hits.
 */
std::vector<Hit> readTimepix3RawData(const std::string &filepath) {
  int chip_layout_type = 0;
  int data_packet_size = 0;
  int data_packet_num = 0;
  unsigned long *tdclast;
  unsigned long mytdc = 0;

  // Open the file
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Error opening file");
  }
  // Read the data from the file into a buffer
  std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
  file.close();

  // Process the buffer to extract the raw data
  // (implementation of this will depend on the specific format of the TimePix3
  // data)
  std::vector<Hit> hits;
  std::cout << "File size: " << buffer.size() << std::endl;

  for (auto i = buffer.begin(); i != buffer.end(); i += 8) {
    std::vector<char> char_array(i, std::next(i, 8));
    if (char_array.size() != 8) {
      std::cout << "EOF, insufficient bytes left." << std::endl;
      break;
    }

    // locate the data packet header
    if (char_array[0] == 'T' && char_array[1] == 'P' && char_array[2] == 'X') {
      // get the size of the data packet
      // NOTE:
      // Sophiread_v1 is using a method that leads to negative data_packet_size
      // and data_packet_num, therefore we are using the code from manufacture
      // example, tpx3cam.cpp to get the data_packet_size.
      data_packet_size = ((0xff & char_array[7]) << 8) | (0xff & char_array[6]);
      data_packet_num =
          data_packet_size >> 3;  // every 8 (2^3) bytes is a data packet

      // get chip layout type
      chip_layout_type = (int)char_array[4];

      // processing each data packet
      for (auto j = 0; j < data_packet_num; j++) {
        i = std::next(i, 8);  // move the iterator to the next data packet
        std::vector<char> data_packet(i, std::next(i, 8));
        if (data_packet.size() != 8) {
          std::cout << "EOF, insufficient bytes left." << std::endl;
          break;
        }
        // extract the data from the data packet
        if (data_packet[7] == 0x6F) {
          // TDC data packets
          tdclast = (unsigned long *)(&data_packet[0]);
          mytdc = (((*tdclast) >> 12) & 0x3fffffff);
        } else if ((data_packet[7] & 0xF0) == 0x40) {
          // GDC
          // do nothing for now (unless we need the GDC count)
        } else if ((data_packet[7] & 0xF0) == 0xb0) {
          // Process the data into hit
          auto hit = packetToHit(data_packet, mytdc, chip_layout_type);
          hits.push_back(hit);
        }
      }
    }
  }

  // Return the hits
  return hits;
}

/**
 * @brief Save labeled hits to HDF5 file.
 *
 * @param out_file_name: output file name.
 * @param hits: hits to be saved.
 * @param labels: cluster ID for each hits.
 */
void saveHitsToHDF5(const std::string out_file_name,
                    const std::vector<Hit> &hits,
                    const std::vector<int> &labels) {
  // sanity check
  if (hits.size() != labels.size()) {
    throw std::runtime_error("Hits and labels must have the same size");
  }

  // write to HDF5 file
  // -- preparation
  H5::H5File out_file(out_file_name, H5F_ACC_TRUNC);
  hsize_t dims[1] = {hits.size()};
  H5::DataSpace dataspace(1, dims);
  H5::IntType int_type(H5::PredType::NATIVE_INT);
  H5::FloatType float_type(H5::PredType::NATIVE_DOUBLE);
  // -- make hits as a group
  H5::Group group = out_file.createGroup("hits");
  // -- write x
  std::vector<int> x(hits.size());
  std::transform(hits.begin(), hits.end(), x.begin(),
                 [](const Hit &hit) { return hit.getX(); });
  H5::DataSet x_dataset = group.createDataSet("x", int_type, dataspace);
  x_dataset.write(x.data(), int_type);
  // -- write y
  std::vector<int> y(hits.size());
  std::transform(hits.begin(), hits.end(), y.begin(),
                 [](const Hit &hit) { return hit.getY(); });
  H5::DataSet y_dataset = group.createDataSet("y", int_type, dataspace);
  y_dataset.write(y.data(), int_type);
  // -- write tot_ns
  std::vector<double> tot_ns(hits.size());
  std::transform(hits.begin(), hits.end(), tot_ns.begin(),
                 [](const Hit &hit) { return hit.getTOT_ns(); });
  H5::DataSet tot_ns_dataset =
      group.createDataSet("tot_ns", float_type, dataspace);
  tot_ns_dataset.write(tot_ns.data(), float_type);
  // -- write toa_ns
  std::vector<double> toa_ns(hits.size());
  std::transform(hits.begin(), hits.end(), toa_ns.begin(),
                 [](const Hit &hit) { return hit.getTOA_ns(); });
  H5::DataSet toa_ns_dataset =
      group.createDataSet("toa_ns", float_type, dataspace);
  toa_ns_dataset.write(toa_ns.data(), float_type);
  // -- write ftoa_ns
  std::vector<double> ftoa_ns(hits.size());
  std::transform(hits.begin(), hits.end(), ftoa_ns.begin(),
                 [](const Hit &hit) { return hit.getFTOA_ns(); });
  H5::DataSet ftoa_ns_dataset =
      group.createDataSet("ftoa_ns", float_type, dataspace);
  ftoa_ns_dataset.write(ftoa_ns.data(), float_type);
  // -- write tof_ns
  std::vector<double> tof_ns(hits.size());
  std::transform(hits.begin(), hits.end(), tof_ns.begin(),
                 [](const Hit &hit) { return hit.getTOF_ns(); });
  H5::DataSet tof_ns_dataset =
      group.createDataSet("tof_ns", float_type, dataspace);
  tof_ns_dataset.write(tof_ns.data(), float_type);
  // -- write spidertime_ns
  std::vector<double> spidertime_ns(hits.size());
  std::transform(hits.begin(), hits.end(), spidertime_ns.begin(),
                 [](const Hit &hit) { return hit.getSPIDERTIME_ns(); });
  H5::DataSet spidertime_ns_dataset =
      group.createDataSet("spidertime_ns", float_type, dataspace);
  spidertime_ns_dataset.write(spidertime_ns.data(), float_type);
  // -- write labels
  H5::DataSet labels_dataset =
      group.createDataSet("labels", int_type, dataspace);
  labels_dataset.write(labels.data(), int_type);
  // -- close file
  out_file.close();
}

/**
 * @brief Save events to HDF5 file.
 *
 * @param out_file_name: output file name.
 * @param events: neutron events to be saved.
 */
void saveEventsToHDF5(const std::string out_file_name,
                      const std::vector<NeutronEvent> &events) {
  // sanity check
  if (events.size() == 0) {
    throw std::runtime_error("No events to save");
  }

  // write to HDF5 file
  // -- preparation
  H5::H5File out_file(out_file_name, H5F_ACC_TRUNC);
  hsize_t dims[1] = {events.size()};
  H5::DataSpace dataspace(1, dims);
  H5::IntType int_type(H5::PredType::NATIVE_INT);
  H5::FloatType float_type(H5::PredType::NATIVE_DOUBLE);
  // -- make events as a group
  H5::Group group = out_file.createGroup("events");
  // -- write x
  std::vector<double> x(events.size());
  std::transform(events.begin(), events.end(), x.begin(),
                 [](const NeutronEvent &event) { return event.getX(); });
  H5::DataSet x_dataset = group.createDataSet("x", float_type, dataspace);
  x_dataset.write(x.data(), float_type);
  // -- write y
  std::vector<double> y(events.size());
  std::transform(events.begin(), events.end(), y.begin(),
                 [](const NeutronEvent &event) { return event.getY(); });
  H5::DataSet y_dataset = group.createDataSet("y", float_type, dataspace);
  y_dataset.write(y.data(), float_type);
  // -- write TOF_ns
  std::vector<double> tof_ns(events.size());
  std::transform(events.begin(), events.end(), tof_ns.begin(),
                 [](const NeutronEvent &event) { return event.getTOF_ns(); });
  H5::DataSet tof_ns_dataset =
      group.createDataSet("tof_ns", float_type, dataspace);
  tof_ns_dataset.write(tof_ns.data(), float_type);
  // -- write Nhits
  std::vector<int> nhits(events.size());
  std::transform(events.begin(), events.end(), nhits.begin(),
                 [](const NeutronEvent &event) { return event.getNHits(); });
  H5::DataSet nhits_dataset = group.createDataSet("NHits", int_type, dataspace);
  nhits_dataset.write(nhits.data(), int_type);
  // -- close file
  out_file.close();
}
