#include "tpx3.h"

#include <H5Cpp.h>

#include <algorithm>
#include <future>
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

std::string Params::toString() const {
  std::stringstream ss;
  ss << "ABS: radius=" << m_abs_radius 
     << ", min_cluster_size=" << m_abs_min_cluster_size
     << ", spider_time_range=" << m_abs_spider_time_range;

  return ss.str();
}

/**
 * @brief Special constructor that construct a Hit from raw bytes.
 *
 * @param packet
 * @param tdc
 * @param gdc
 * @param chip_layout_type
 */
Hit::Hit(const char *packet, const unsigned long long tdc, const unsigned long long gdc, const int chip_layout_type) {
  unsigned short pixaddr, dcol, spix, pix;
  unsigned short *spider_time;
  unsigned short *nTOT;    // bytes 2,3, raw time over threshold
  unsigned int *nTOA;      // bytes 3,4,5,6, raw time of arrival
  unsigned int *npixaddr;  // bytes 4,5,6,7
  unsigned int spidertime = 0;
  // timing information
  spider_time = (unsigned short *)(&packet[0]);  // Spider time  (16 bits)
  nTOT = (unsigned short *)(&packet[2]);         // ToT          (10 bits)
  nTOA = (unsigned int *)(&packet[3]);           // ToA          (14 bits)
  m_ftoa = *nTOT & 0xF;                          // fine ToA     (4 bits)
  m_tot = (*nTOT >> 4) & 0x3FF;
  m_toa = (*nTOA >> 6) & 0x3FFF;
  spidertime = 16384 * (*spider_time) + m_toa;

  // rename variables for clarity
  unsigned long long int GDC_timestamp = gdc;
  unsigned long long TDC_timestamp = tdc;

  // convert spidertime to global timestamp
  unsigned long SPDR_LSB30 = 0;
  unsigned long SPDR_MSB18 = 0;
  //   unsigned long long SPDR_timestamp = 0;

  SPDR_LSB30 = GDC_timestamp & 0x3FFFFFFF;
  SPDR_MSB18 = (GDC_timestamp >> 30) & 0x3FFFF;
  if (spidertime < SPDR_LSB30) {
    SPDR_MSB18++;
  }
  m_spidertime = (SPDR_MSB18 << 30) & 0xFFFFC0000000;
  m_spidertime = m_spidertime | spidertime;

  // tof calculation
  // TDC packets not always arrive before corresponding data packets
  if (m_spidertime < TDC_timestamp) {
    m_tof = m_spidertime - TDC_timestamp + 16666667;  // 1E9 / 60.0 is approximately 16666667
  } else {
    m_tof = m_spidertime - TDC_timestamp;
  }

  // pixel address
  npixaddr = (unsigned int *)(&packet[4]);  // Pixel address (14 bits)
  pixaddr = (*npixaddr >> 12) & 0xFFFF;
  dcol = ((pixaddr & 0xFE00) >> 8);
  spix = ((pixaddr & 0x1F8) >> 1);
  pix = pixaddr & 0x7;
  m_x = dcol + (pix >> 2);   // x coordinate
  m_y = spix + (pix & 0x3);  // y coordinate
  // adjustment for chip layout
  if (chip_layout_type == 0) {  // single
    m_x += 260;
    // m_y = m_y;
  } else if (chip_layout_type == 1) {  // double
    m_x = 255 - m_x + 260;
    m_y = 255 - m_y + 260;
  } else if (chip_layout_type == 2) {  // triple
    m_x = 255 - m_x;
    m_y = 255 - m_y + 260;
  }
}

/**
 * @brief Alternative way to parse data packet where timing is assumed.
 *
 * @param packet
 * @param rollover_counter
 * @param previous_time
 * @param chip_layout_type
 * @return Hit
 *
 * @note As of 2023-02-24, timing packet (gdc, tdc) are not reliable, therefore
 * this function is used as a temporary solution with assumed timing, until
 * timing packet is fixed on the hardware side.
 */
Hit packetToHitAlt(const std::vector<char> &packet,
                   unsigned long long *rollover_counter,
                   unsigned long long *previous_time,
                   const int chip_layout_type) {
  unsigned short pixaddr, dcol, spix, pix;
  unsigned short *spider_time;
  unsigned short *nTOT;    // bytes 2,3, raw time over threshold
  unsigned int *nTOA;      // bytes 3,4,5,6, raw time of arrival
  unsigned int *npixaddr;  // bytes 4,5,6,7
  int x, y, tot, toa, ftoa;
  unsigned int spidertime, tof;
  // timing information
  spider_time = (unsigned short *)(&packet[0]);  // Spider time  (16 bits)
  nTOT = (unsigned short *)(&packet[2]);         // ToT          (10 bits)
  nTOA = (unsigned int *)(&packet[3]);           // ToA          (14 bits)
  ftoa = *nTOT & 0xF;                            // fine ToA     (4 bits)
  tot = (*nTOT >> 4) & 0x3FF;
  toa = (*nTOA >> 6) & 0x3FFF;
  spidertime = 16384 * (*spider_time) + toa;

  // time calculation
  // Compute SPDR_timestamp
  unsigned long long SPDR_timestamp = 0;
  const unsigned long long time_range = 1 << 30;

  // if curr hit arrives earlier than previous hit (out of order)
  // if it is a lot earlier, it belongs to the next rollover

  if (spidertime < *previous_time) {
    if (*previous_time - spidertime > time_range / 2) {
      *rollover_counter += 1;
    }

  // if the curr hit arrives later than previous hit (in order)
  // if it is a lot later, it belongs to the previous rollover
  } else {
    if (spidertime - *previous_time > time_range / 2) {
      if (*rollover_counter > 0) {
        *rollover_counter -= 1;
      }
    }
  }

  *previous_time = spidertime;

  SPDR_timestamp = spidertime + (*rollover_counter) * time_range;
  // compute tof = mod(SPDR_timestamp, 666667)
  // 666667 = 1E9/60.0/25.0
  // a consistent round off error of 10ns due to using integer for modulus
  // which is way below the 100ns time resolution needed
  tof = SPDR_timestamp % 666667;
  
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
  return Hit(x, y, tot, toa, ftoa, tof, SPDR_timestamp);
}

/**
 * @brief Convert a raw data packet into a hit.
 *
 * @param packet: raw data packet
 * @param tdc: time stamp collected from the monitor
 * @param chip_layout_type: chip layout ID number
 * @return Hit
 */
Hit packetToHit(const std::vector<char> &packet, const unsigned long long tdc,
                const unsigned long long gdc, const int chip_layout_type) {
  unsigned short pixaddr, dcol, spix, pix;
  unsigned short *spider_time;
  unsigned short *nTOT;    // bytes 2,3, raw time over threshold
  unsigned int *nTOA;      // bytes 3,4,5,6, raw time of arrival
  unsigned int *npixaddr;  // bytes 4,5,6,7
  int x, y, tot, toa, ftoa;
  unsigned int spidertime=0, tof=0;
  // timing information
  spider_time = (unsigned short *)(&packet[0]);  // Spider time  (16 bits)
  nTOT = (unsigned short *)(&packet[2]);         // ToT          (10 bits)
  nTOA = (unsigned int *)(&packet[3]);           // ToA          (14 bits)
  ftoa = *nTOT & 0xF;                            // fine ToA     (4 bits)
  tot = (*nTOT >> 4) & 0x3FF;
  toa = (*nTOA >> 6) & 0x3FFF;
  spidertime = 16384 * (*spider_time) + toa;

  // rename variables for clarity
  unsigned long long int GDC_timestamp = gdc;
  unsigned long long TDC_timestamp = tdc;

  // convert spidertime to global timestamp
  unsigned long SPDR_LSB30 = 0;
  unsigned long SPDR_MSB18 = 0;
  unsigned long long SPDR_timestamp = 0;

  SPDR_LSB30 = GDC_timestamp & 0x3FFFFFFF;
  SPDR_MSB18 = (GDC_timestamp >> 30) & 0x3FFFF;
  if (spidertime < SPDR_LSB30) {
    SPDR_MSB18++;
  }
  SPDR_timestamp = (SPDR_MSB18 << 30) & 0xFFFFC0000000;
  SPDR_timestamp = SPDR_timestamp | spidertime;

  // tof calculation
  // TDC packets not always arrive before corresponding data packets
  if (SPDR_timestamp < TDC_timestamp){
    tof = SPDR_timestamp - TDC_timestamp + 1E9/60.0;
  } else {
    tof = SPDR_timestamp - TDC_timestamp;
  }

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
  return Hit(x, y, tot, toa, ftoa, tof, SPDR_timestamp);
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

  // for Alternative timing handler
  unsigned long long *rollover_counter = new unsigned long long(0);
  unsigned long long *previous_time = new unsigned long long(0);

  // for TDC information
  unsigned long *tdclast;
  unsigned long long mytdc = 0;
  unsigned long TDC_MSB16 = 0;
  unsigned long TDC_LSB32 = 0;
  unsigned long TDC_timestamp = 0;

  // for GDC information
  unsigned long *gdclast;
  unsigned long long mygdc = 0;
  unsigned long Timer_LSB32 = 0;
  unsigned long Timer_MSB16 = 0;
  unsigned long long int GDC_timestamp = 0;  // 48-bit

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
          mytdc = (((*tdclast) >> 12) &
                   0xFFFFFFFF);  // rick: 0x3fffffff, get 32-bit tdc
          TDC_LSB32 = GDC_timestamp & 0xFFFFFFFF;
          TDC_MSB16 = (GDC_timestamp >> 32) & 0xFFFF;
          if (mytdc < TDC_LSB32) {
            TDC_MSB16++;
          }
          TDC_timestamp = (TDC_MSB16 << 32) & 0xFFFF00000000;
          TDC_timestamp = TDC_timestamp | mytdc;
          // std::cout << "TDC_timestamp: " << std::setprecision(15) <<
          // TDC_timestamp*25E-9 <<std::endl;
        } else if ((data_packet[7] & 0xF0) == 0x40) {
          // GDC data packet
          gdclast = (unsigned long *)(&data_packet[0]);
          mygdc = (((*gdclast) >> 16) & 0xFFFFFFFFFFF);
          if (((mygdc >> 40) & 0xF) == 0x4) {
            Timer_LSB32 = mygdc & 0xFFFFFFFF;  // 32-bit
          } else if (((mygdc >> 40) & 0xF) == 0x5) {
            Timer_MSB16 = mygdc & 0xFFFF;  // 16-bit
            GDC_timestamp = Timer_MSB16;
            GDC_timestamp = (GDC_timestamp << 32) & 0xFFFF00000000;
            GDC_timestamp = GDC_timestamp | Timer_LSB32;
            // std::cout << "GDC_timestamp: " << std::setprecision(15) <<
            // GDC_timestamp*25E-9 << std::endl;
          }
        } else if ((data_packet[7] & 0xF0) == 0xb0) {
          // NOTE: as of 2023-02-24, timing data packet cannot be used, using
          // alternative method to get the timing information
          auto hit = packetToHitAlt(data_packet, rollover_counter,
                                    previous_time, chip_layout_type);
          // std::cout << hit.toString() << std::endl;
          // Process the data into hit
          // auto hit = packetToHit(data_packet, TDC_timestamp, GDC_timestamp,
          //                        chip_layout_type);
          // std::cout << "Hits: " << hit.getX() << " " << hit.getY() << " " <<
          // hit.getTOF_ns()*1E-6 << " " << hit.getSPIDERTIME_ns()*1E-9 <<
          // std::endl;
          // std::cout << std::setprecision(15) << hit.getSPIDERTIME_ns()*1E-9 << std::endl;
          hits.push_back(hit);
        }
      }
    }
  }

  // Return the hits
  return hits;
}

/**
 * @brief Read the raw data from a TimePix3 file, main thread is only collecting
 *        header positions (i.e. packets batch), and the work is distributed to
 *        other worker threads to process each packets into hits.
 *
 * @param raw_bytes
 * @param num_threads
 * @return std::vector<Hit>
 */
std::vector<Hit> fastParseTPX3Raw(const std::vector<char> &raw_bytes, int num_threads) {
  std::vector<Hit> hits;
  hits.reserve(raw_bytes.size() / 64);

  std::vector<TPX3H> batches;
  batches.reserve(raw_bytes.size() / 128);  // just a guess here, need more work

  // local variables
  int chip_layout_type = 0;
  int data_packet_size = 0;
  int data_packet_num = 0;

  // find all batches
  for (auto i = raw_bytes.begin(); i + 8 < raw_bytes.end(); i += 8) {
    const char *char_array = &(*i);

    // locate the data packet header
    if (char_array[0] == 'T' && char_array[1] == 'P' && char_array[2] == 'X') {
      data_packet_size = ((0xff & char_array[7]) << 8) | (0xff & char_array[6]);
      data_packet_num = data_packet_size >> 3;  // every 8 (2^3) bytes is a data packet
      chip_layout_type = static_cast<int>(char_array[4]);
      batches.emplace_back(TPX3H{i, data_packet_size, data_packet_num, chip_layout_type});
    }
  }

  // process batches in multiple threads
  std::vector<std::future<std::vector<Hit>>> futures;

  for (int i = 0; i < num_threads; i++) {
    int start = i * batches.size() / num_threads;
    int end = (i + 1) * batches.size() / num_threads;

    futures.push_back(std::async(std::launch::async, [=, &raw_bytes] {
      std::vector<Hit> thread_hits;
      for (int j = start; j < end; j++) {
        auto hits_batch = processBatch(batches[j], raw_bytes);
        thread_hits.insert(thread_hits.end(), hits_batch.begin(), hits_batch.end());
      }
      return thread_hits;
    }));
  }

  for (auto &future : futures) {
    auto future_hits = future.get();
    hits.insert(hits.end(), future_hits.begin(), future_hits.end());
  }

  return hits;
}

std::vector<Hit> processBatch(TPX3H batch, const std::vector<char> &raw_bytes) {
  //
  std::vector<Hit> hits;
  hits.reserve(batch.num_packets);

  // -- TDC
  unsigned long *tdclast;
  unsigned long long mytdc = 0;
  unsigned long TDC_MSB16 = 0;
  unsigned long TDC_LSB32 = 0;
  unsigned long TDC_timestamp = 0;
  // -- GDC
  unsigned long *gdclast;
  unsigned long long mygdc = 0;
  unsigned long Timer_LSB32 = 0;
  unsigned long Timer_MSB16 = 0;
  unsigned long long int GDC_timestamp = 0;  // 48-bit

  //
  auto i = batch.header_it;
  for (auto j = 0; j < batch.num_packets; ++j) {
    if (i + 8 > raw_bytes.end()) {
      continue;
    }

    i = std::next(i, 8);
    const char *char_array = &(*i);

    // extract the data from the data packet
    if (char_array[7] == 0x6F) {
      // TDC data packets
      tdclast = (unsigned long *)(&char_array[0]);
      mytdc = (((*tdclast) >> 12) & 0xFFFFFFFF);  // rick: 0x3fffffff, get 32-bit tdc
      TDC_LSB32 = GDC_timestamp & 0xFFFFFFFF;
      TDC_MSB16 = (GDC_timestamp >> 32) & 0xFFFF;
      if (mytdc < TDC_LSB32) {
        TDC_MSB16++;
      }
      TDC_timestamp = (TDC_MSB16 << 32) & 0xFFFF00000000;
      TDC_timestamp = TDC_timestamp | mytdc;
    } else if ((char_array[7] & 0xF0) == 0x40) {
      // GDC data packet
      gdclast = (unsigned long *)(&char_array[0]);
      mygdc = (((*gdclast) >> 16) & 0xFFFFFFFFFFF);
      if (((mygdc >> 40) & 0xF) == 0x4) {
        Timer_LSB32 = mygdc & 0xFFFFFFFF;  // 32-bit
      } else if (((mygdc >> 40) & 0xF) == 0x5) {
        Timer_MSB16 = mygdc & 0xFFFF;  // 16-bit
        GDC_timestamp = Timer_MSB16;
        GDC_timestamp = (GDC_timestamp << 32) & 0xFFFF00000000;
        GDC_timestamp = GDC_timestamp | Timer_LSB32;
      }
    } else if ((char_array[7] & 0xF0) == 0xb0) {
      // record the packet info
      hits.emplace_back(Hit(char_array, TDC_timestamp, GDC_timestamp, batch.chip_layout_type));
    }
  }

  return hits;
}

/**
 * @brief Iterate through the raw char array and convert it to hits.
 *
 * @param raw_bytes
 * @return std::vector<Hit>
 */
std::vector<Hit> parseRawBytesToHits(const std::vector<char> &raw_bytes) {
  // assume every 64 char will result in 1 hit (this needs to be verified via
  // experiments).
  std::vector<Hit> hits;
  hits.reserve(raw_bytes.size());

  // local variables
  // -- packet information
  int chip_layout_type = 0;
  int data_packet_size = 0;
  int data_packet_num = 0;
  // -- TDC
  unsigned long *tdclast;
  unsigned long long mytdc = 0;
  unsigned long TDC_MSB16 = 0;
  unsigned long TDC_LSB32 = 0;
  unsigned long TDC_timestamp = 0;
  // -- GDC
  unsigned long *gdclast;
  unsigned long long mygdc = 0;
  unsigned long Timer_LSB32 = 0;
  unsigned long Timer_MSB16 = 0;
  unsigned long long int GDC_timestamp = 0;  // 48-bit

  // main loop
  for (auto i = raw_bytes.begin(); i + 8 < raw_bytes.end(); i += 8) {
    const char *char_array = &(*i);

    // locate the data packet header
    if (char_array[0] == 'T' && char_array[1] == 'P' && char_array[2] == 'X') {
      // get the size of the data packet
      data_packet_size = ((0xff & char_array[7]) << 8) | (0xff & char_array[6]);
      data_packet_num = data_packet_size >> 3;  // every 8 (2^3) bytes is a data packet
      // get chip layout type
      chip_layout_type = static_cast<int>(char_array[4]);

      // processing each data packet
      for (auto j = 0; j < data_packet_num; ++j) {
        if (i + 8 > raw_bytes.end()) {
          continue;
        }

        i = std::next(i, 8);  // move the iterator to the next data packet
        char_array = &(*i);

        // extract the data from the data packet
        if (char_array[7] == 0x6F) {
          // TDC data packets
          tdclast = (unsigned long *)(&char_array[0]);
          mytdc = (((*tdclast) >> 12) & 0xFFFFFFFF);  // rick: 0x3fffffff, get 32-bit tdc
          TDC_LSB32 = GDC_timestamp & 0xFFFFFFFF;
          TDC_MSB16 = (GDC_timestamp >> 32) & 0xFFFF;
          if (mytdc < TDC_LSB32) {
            TDC_MSB16++;
          }
          TDC_timestamp = (TDC_MSB16 << 32) & 0xFFFF00000000;
          TDC_timestamp = TDC_timestamp | mytdc;
        } else if ((char_array[7] & 0xF0) == 0x40) {
          // GDC data packet
          gdclast = (unsigned long *)(&char_array[0]);
          mygdc = (((*gdclast) >> 16) & 0xFFFFFFFFFFF);
          if (((mygdc >> 40) & 0xF) == 0x4) {
            Timer_LSB32 = mygdc & 0xFFFFFFFF;  // 32-bit
          } else if (((mygdc >> 40) & 0xF) == 0x5) {
            Timer_MSB16 = mygdc & 0xFFFF;  // 16-bit
            GDC_timestamp = Timer_MSB16;
            GDC_timestamp = (GDC_timestamp << 32) & 0xFFFF00000000;
            GDC_timestamp = GDC_timestamp | Timer_LSB32;
          }
        } else if ((char_array[7] & 0xF0) == 0xb0) {
          // Process the data into hit
          hits.emplace_back(Hit(char_array, TDC_timestamp, GDC_timestamp, chip_layout_type));
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
void saveHitsToHDF5(const std::string out_file_name, const std::vector<Hit> &hits, const std::vector<int> &labels) {
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
  std::transform(hits.begin(), hits.end(), x.begin(), [](const Hit &hit) { return hit.getX(); });
  H5::DataSet x_dataset = group.createDataSet("x", int_type, dataspace);
  x_dataset.write(x.data(), int_type);
  // -- write y
  std::vector<int> y(hits.size());
  std::transform(hits.begin(), hits.end(), y.begin(), [](const Hit &hit) { return hit.getY(); });
  H5::DataSet y_dataset = group.createDataSet("y", int_type, dataspace);
  y_dataset.write(y.data(), int_type);
  // -- write tot_ns
  std::vector<double> tot_ns(hits.size());
  std::transform(hits.begin(), hits.end(), tot_ns.begin(), [](const Hit &hit) { return hit.getTOT_ns(); });
  H5::DataSet tot_ns_dataset = group.createDataSet("tot_ns", float_type, dataspace);
  tot_ns_dataset.write(tot_ns.data(), float_type);
  // -- write toa_ns
  std::vector<double> toa_ns(hits.size());
  std::transform(hits.begin(), hits.end(), toa_ns.begin(), [](const Hit &hit) { return hit.getTOA_ns(); });
  H5::DataSet toa_ns_dataset = group.createDataSet("toa_ns", float_type, dataspace);
  toa_ns_dataset.write(toa_ns.data(), float_type);
  // -- write ftoa_ns
  std::vector<double> ftoa_ns(hits.size());
  std::transform(hits.begin(), hits.end(), ftoa_ns.begin(), [](const Hit &hit) { return hit.getFTOA_ns(); });
  H5::DataSet ftoa_ns_dataset = group.createDataSet("ftoa_ns", float_type, dataspace);
  ftoa_ns_dataset.write(ftoa_ns.data(), float_type);
  // -- write tof_ns
  std::vector<double> tof_ns(hits.size());
  std::transform(hits.begin(), hits.end(), tof_ns.begin(), [](const Hit &hit) { return hit.getTOF_ns(); });
  H5::DataSet tof_ns_dataset = group.createDataSet("tof_ns", float_type, dataspace);
  tof_ns_dataset.write(tof_ns.data(), float_type);
  // -- write spidertime_ns
  std::vector<double> spidertime_ns(hits.size());
  std::transform(hits.begin(), hits.end(), spidertime_ns.begin(),
                 [](const Hit &hit) { return hit.getSPIDERTIME_ns(); });
  H5::DataSet spidertime_ns_dataset = group.createDataSet("spidertime_ns", float_type, dataspace);
  spidertime_ns_dataset.write(spidertime_ns.data(), float_type);
  // -- write labels
  H5::DataSet labels_dataset = group.createDataSet("labels", int_type, dataspace);
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
void saveEventsToHDF5(const std::string out_file_name, const std::vector<NeutronEvent> &events) {
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
  std::transform(events.begin(), events.end(), x.begin(), [](const NeutronEvent &event) { return event.getX(); });
  H5::DataSet x_dataset = group.createDataSet("x", float_type, dataspace);
  x_dataset.write(x.data(), float_type);
  // -- write y
  std::vector<double> y(events.size());
  std::transform(events.begin(), events.end(), y.begin(), [](const NeutronEvent &event) { return event.getY(); });
  H5::DataSet y_dataset = group.createDataSet("y", float_type, dataspace);
  y_dataset.write(y.data(), float_type);
  // -- write TOF_ns
  std::vector<double> tof_ns(events.size());
  std::transform(events.begin(), events.end(), tof_ns.begin(),
                 [](const NeutronEvent &event) { return event.getTOF_ns(); });
  H5::DataSet tof_ns_dataset = group.createDataSet("tof_ns", float_type, dataspace);
  tof_ns_dataset.write(tof_ns.data(), float_type);
  // -- write Nhits
  std::vector<int> nhits(events.size());
  std::transform(events.begin(), events.end(), nhits.begin(),
                 [](const NeutronEvent &event) { return event.getNHits(); });
  H5::DataSet nhits_dataset = group.createDataSet("NHits", int_type, dataspace);
  nhits_dataset.write(nhits.data(), int_type);
  // -- write TOT
  std::vector<double> tot(events.size());
  std::transform(events.begin(), events.end(), tot.begin(), [](const NeutronEvent &event) { return event.getTOT(); });
  H5::DataSet tot_dataset = group.createDataSet("tot", float_type, dataspace);
  tot_dataset.write(tot.data(), float_type);
  // -- close file
  out_file.close();
}

  /**
   * @brief Parse user-defined parameters from a parameter file
   *
   * @param filepath: path to the parameter file.
   * @return Params
   */

  Params parseUserDefinedParams(const std::string &filepath) {
    // default ABS settings
    double radius = 5.0;
    unsigned long int min_cluster_size = 1;
    unsigned long int spider_time_range = 75;

    std::ifstream user_defined_params_file(filepath);
    std::string line;

    while (std::getline(user_defined_params_file, line)) {
      std::istringstream ss(line);
      std::string name;
      ss >> name;
      if (name == "abs_radius") {
      ss >> radius;
      } else if (name == "abs_min_cluster_size") {
      ss >> min_cluster_size;
      } else if (name == "spider_time_range") {
      ss >> spider_time_range;
      }
    }

    Params p(radius, min_cluster_size, spider_time_range);

    // prints out user-defined parameters
    std::cout << "User-defined params file: " << filepath << std::endl;
    std::cout << p.toString() << std::endl;

    return p;
  }
