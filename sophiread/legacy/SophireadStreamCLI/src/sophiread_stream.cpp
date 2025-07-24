/**
 * @brief CLI demo case for using the streaming API to process raw data.
 *
 */
#include <readerwriterqueue.h>

#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#include "abs.h"
#include "dbscan.h"
#include "tpx3.h"

moodycamel::BlockingReaderWriterQueue<Hit> q_hits;
bool done = false;
const int batch_size = 10000;
std::vector<NeutronEvent> neutron_events;

/**
 * @brief Cluster the batch of hits into neutron events
 *
 * @param batch
 */
void process_batch(const std::vector<Hit> &batch) {
  ClusteringAlgorithm *alg;
  alg = new ABS(5.0, 1, 75);         // select clustering algorithm
  alg->set_method("fast_gaussian");  // select peak fitting method
  alg->fit(batch);
  std::vector<NeutronEvent> events = alg->get_events(batch);
  for (auto event : events) {
    neutron_events.push_back(event);
  }
}

/**
 * @brief Read the raw data from tpx3 file, convert to hit and enqueue to queue
 *
 * @param filename
 */
void reader(std::string filename) {
  // local vars
  int chip_layout_type = 0;
  int data_packet_size = 0;
  int data_packet_num = 0;

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

  // sanity check
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return;
  }

  std::cout << "Reading file: " << filename << std::endl;

  while (!file.eof()) {
    unsigned char buffer[8];
    file.read(reinterpret_cast<char *>(buffer), 8);

    // locate the data packet header
    if (buffer[0] == 'T' && buffer[1] == 'P' && buffer[2] == 'X') {
      // chip type
      chip_layout_type = (int)buffer[4];
      // read the data packet size
      data_packet_size = ((0xff & buffer[7]) << 8) | (0xff & buffer[6]);
      data_packet_num = data_packet_size >> 3;

      // process each data section
      for (auto j = 0; j < data_packet_num; j++) {
        // read the data packet
        unsigned char char_array[8];
        file.read(reinterpret_cast<char *>(char_array), 8);
        // extract the data from the data packet
        if (char_array[7] == 0x6F) {
          // TDC data packets
          tdclast = (unsigned long *)(&char_array[0]);
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
            // std::cout << "GDC_timestamp: " << std::setprecision(15) <<
            // GDC_timestamp*25E-9 << std::endl;
          }
        } else if ((char_array[7] & 0xF0) == 0xb0) {
          // Process the data into hit
          auto data_packet = std::vector<char>(
              char_array, char_array + sizeof(char_array) / sizeof(char));
          auto hit = packetToHit(data_packet, TDC_timestamp, GDC_timestamp,
                                 chip_layout_type);
          // std::cout << "Hits: " << hit.getX() << " " << hit.getY() << " " <<
          // hit.getTOF_ns()*1E-6 << " " << hit.getSPIDERTIME_ns()*1E-9 <<
          // std::endl;
          q_hits.enqueue(hit);
        }
        //
      }
      // END OF PROCESS DATA SECTION
    }
  }
  done = true;
}

/**
 * @brief Process the data from queue in batches (concurrently)
 *
 */
void processor() {
  std::vector<Hit> batch;
  while (!done) {
    Hit hit(0, 0, 0, 0, 0, 0, 0);
    if (q_hits.try_dequeue(hit)) {
      batch.push_back(hit);
      if (batch.size() >= batch_size) {
        // process data
        process_batch(batch);
        batch.clear();
      }
    }
  }

  // process remaining data
  batch.clear();
  while (q_hits.size_approx() > 0) {
    Hit hit(0, 0, 0, 0, 0, 0, 0);
    q_hits.wait_dequeue(hit);
    batch.push_back(hit);
  }
  if (batch.size() > 0) {
    process_batch(batch);
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: ./SophireadStream <filename>" << std::endl;
    return 1;
  }

  std::string filename = argv[1];

  std::thread reader_thread(reader, filename);
  std::thread processor_thread(processor);

  reader_thread.join();
  processor_thread.join();

  // print out events
  // for (auto event : neutron_events) {
  //   std::cout << event.toString() << std::endl;
  // }
  std::cout << "Number of events: " << neutron_events.size() << std::endl;

  // save events to file
  saveEventsToHDF5("events_streamed.h5", neutron_events);

  return 0;
}
