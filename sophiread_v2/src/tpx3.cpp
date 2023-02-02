#include "tpx3.h"

#include <iostream>
#include <sstream>

Hit::Hit(const int x, const int y, const int tot, const int toa, const int ftoa,
         const unsigned int tof, const int spidertime)
    : m_x(x),
      m_y(y),
      m_tot(tot),
      m_toa(toa),
      m_ftoa(ftoa),
      m_tof(tof),
      m_spidertime(spidertime) {}

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

std::vector<Hit> readTimepix3RawData(const std::string &filepath) {
  int chip_layout_type = 0;
  int data_packet_size = 0;
  int data_packet_num = 0;
  unsigned long *tdclast;
  unsigned long mytdc = 0;
  unsigned short pixaddr, dcol, spix, pix;
  unsigned short *spider_time;
  unsigned short *nTOT;    // bytes 2,3, raw time over threshold
  unsigned int *nTOA;      // bytes 3,4,5,6, raw time of arrival
  unsigned int *npixaddr;  // bytes 4,5,6,7
  int x, y, tot, toa, ftoa, spidertime;
  unsigned int tof;

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

      // processing each data packet
      for (auto j = 0; j < data_packet_num; j++) {
        i = std::next(i, 8);  // move the iterator to the next data packet
        std::vector<char> data_packet(i, std::next(i, 8));
        if (data_packet.size() != 8) {
          std::cout << "EOF, insufficient bytes left." << std::endl;
          break;
        }
        chip_layout_type = (int)char_array[4];
        // extract the data from the data packet
        if (data_packet[7] == 0x6F) {
          // TDC data packets
          tdclast = (unsigned long *)(&data_packet[0]);
          mytdc = (((*tdclast) >> 12) & 0x3fffffff);
        } else if ((data_packet[7] & 0xF0) == 0x40) {
          // GDC
          // do nothing for now (unless we need the GDC count)
        } else if ((data_packet[7] & 0xF0) == 0xb0) {
          // timing section
          spider_time =
              (unsigned short *)(&data_packet[0]);     // Spider time  (16 bits)
          nTOT = (unsigned short *)(&data_packet[2]);  // ToT          (10 bits)
          nTOA = (unsigned int *)(&data_packet[3]);    // ToA          (14 bits)
          ftoa = *nTOT & 0xF;                          // fine ToA     (4 bits)
          tot = (*nTOT >> 4) & 0x3FF;
          toa = (*nTOA >> 6) & 0x3FFF;
          spidertime = 16384 * (*spider_time) + toa;
          tof = spidertime - mytdc;

          // position section
          npixaddr =
              (unsigned int *)(&data_packet[4]);  // Pixel address (14 bits)
          pixaddr = (*npixaddr >> 12) & 0xFFFF;
          dcol = ((pixaddr & 0xFE00) >> 8);
          spix = ((pixaddr & 0x1F8) >> 1);
          pix = pixaddr & 0x7;
          x = dcol + (pix >> 2);   // x coordinate
          y = spix + (pix & 0x3);  // y coordinate

          // update pixel position based on chip layout type
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

          // make the hit
          hits.push_back(Hit(x, y, tot, toa, ftoa, tof, spidertime));
        }
      }
    }
  }

  // Return the hits
  return hits;
}

/**
 * @brief Save labeled hits to HDF5 file
 *
 * @param out_file_name
 * @param hits
 * @param labels
 */
void saveHitsToHDF5(const std::string out_file_name,
                    const std::vector<Hit> &hits,
                    const std::vector<int> &labels) {
  // sanity check
  if (hits.size() != labels.size()) {
    throw std::runtime_error("Hits and labels must have the same size");
  }
}

/**
 * @brief Save events to HDF5 file
 *
 * @param out_file_name
 * @param events
 */
void saveEventsToHDF5(const std::string out_file_name,
                      const std::vector<NeutronEvent> &events) {}
