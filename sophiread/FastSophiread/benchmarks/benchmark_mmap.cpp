/**
 * @file benchmark_mmap.cpp
 * @author Chen Zhang (zhangc@ornl.gov) + contributors
 * @brief Demonstrate memory-mapped processing converting raw data to events and output.
 * @version 0.2.1
 * @date 2024-01-30
 *
 * @copyright Copyright (c) 2023
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <chrono>
#include <cstdint>  // for std::numeric_limits<>
#include <fstream>
#include <iostream>

#include "abs.h"
#include "disk_io.h"
#include "hit.h"
#include "spdlog/spdlog.h"
#include "tbb/tbb.h"
#include "tpx3_fast.h"

// from SophireadLib/include/tpx3.cpp
#include <H5Cpp.h>

#include "neutron.h"
void saveEventsToHDF5(const std::string out_file_name, const std::vector<Neutron>& events);
/**
 * @brief Save events to HDF5 file.
 *
 * NOTE: it does not appear that this function produces reliable output data.
 * The files are not identical from run-to-run (despite the events being identical)
 * The resulting .h5 file does not load correctly in hdfview (at least I don't know how to see the vectors in it).
 *
 * TODO: There is another program Sophiread that also writes out events.  Make sure that those match up.
 * Better still, use common output writing routines for both programs.
 *
 * @param out_file_name: output file name.
 * @param events: neutron events to be saved.
 */
void saveEventsToHDF5(const std::string out_file_name, const std::vector<Neutron>& events) {
  // sanity check
  if (events.size() == 0) return;

  // write to HDF5 file
  // -- preparation
  H5::H5File out_file(out_file_name, H5F_ACC_TRUNC);
  hsize_t dims[1] = {events.size()};
  H5::DataSpace dataspace(1, dims);
  H5::IntType int_type(H5::PredType::NATIVE_INT);
  H5::FloatType float_type(H5::PredType::NATIVE_DOUBLE);
  // -- make events as a group
  H5::Group group = out_file.createGroup("neutrons");
  // -- write x
  std::vector<double> x(events.size());
  std::transform(events.begin(), events.end(), x.begin(), [](const Neutron& event) { return event.getX(); });
  H5::DataSet x_dataset = group.createDataSet("x", float_type, dataspace);
  x_dataset.write(x.data(), float_type);
  // -- write y
  std::vector<double> y(events.size());
  std::transform(events.begin(), events.end(), y.begin(), [](const Neutron& event) { return event.getY(); });
  H5::DataSet y_dataset = group.createDataSet("y", float_type, dataspace);
  y_dataset.write(y.data(), float_type);
  // -- write TOF_ns
  std::vector<double> tof_ns(events.size());
  std::transform(events.begin(), events.end(), tof_ns.begin(), [](const Neutron& event) { return event.getTOF_ns(); });
  H5::DataSet tof_ns_dataset = group.createDataSet("tof", float_type, dataspace);
  tof_ns_dataset.write(tof_ns.data(), float_type);
  // -- write Nhits
  std::vector<int> nhits(events.size());
  std::transform(events.begin(), events.end(), nhits.begin(), [](const Neutron& event) { return event.getNHits(); });
  H5::DataSet nhits_dataset = group.createDataSet("nHits", int_type, dataspace);
  nhits_dataset.write(nhits.data(), int_type);
  // -- write TOT
  std::vector<double> tot(events.size());
  std::transform(events.begin(), events.end(), tot.begin(), [](const Neutron& event) { return event.getTOT(); });
  H5::DataSet tot_dataset = group.createDataSet("tot", float_type, dataspace);
  tot_dataset.write(tot.data(), float_type);
  // -- close file
  out_file.close();
}

void saveEventsToCSV(const std::string out_file_name, const std::vector<Neutron>& events);
/**
 * @brief Save events to CSV file.
 *
 * @param out_file_name: output file name.
 * @param events: neutron events to be saved.
 */
void saveEventsToCSV(const std::string out_file_name, const std::vector<Neutron>& events) {
  // sanity check
  if (events.size() == 0) return;

  // write to CSV file
  // -- preparation and header
  std::ofstream file(out_file_name, std::ofstream::app);

  // Check if file is open successfully
  if (!file.is_open()) {
    spdlog::error("Failed to open file: {}", out_file_name);
    exit(EXIT_FAILURE);
  }

  file << "X,Y,TOF (ns),Nhits, TOT\n";

  // write events
  for (auto& event : events) {
    file << event.getX() << "," << event.getY() << "," << event.getTOF_ns() << "," << event.getNHits() << ","
         << event.getTOT() << "\n";
  }

  // -- close file
  file.close();
}

void saveEventsToBIN(const std::string out_file_name, const std::vector<Neutron>& events);
/**
 * @brief Save events to BIN file.
 *
 * @param out_file_name: output file name.
 * @param events: neutron events to be saved.
 */
void saveEventsToBIN(const std::string out_file_name, const std::vector<Neutron>& events) {
  // sanity check
  if (events.size() == 0) return;

  // write to BIN file
  // -- preparation and header
  std::ofstream file(out_file_name, std::ofstream::binary | std::ofstream::trunc);

  // Check if file is open successfully
  if (!file.is_open()) {
    spdlog::error("Failed to open file: {}", out_file_name);
    exit(EXIT_FAILURE);
  }

  // write events
  for (auto& event : events) {
    std::uint16_t x = (std::uint16_t)(event.getX() * std::numeric_limits<uint16_t>::max() / 512.0);
    std::uint16_t y = (std::uint16_t)(event.getY() * std::numeric_limits<uint16_t>::max() / 512.0);
    std::uint64_t tof = (std::uint64_t)event.getTOF_ns();
    std::uint16_t nhits = (std::uint16_t)event.getNHits();
    std::uint16_t tot = (std::uint16_t)event.getTOT();

    file << x << y << tof << nhits << tot;
  }

  // -- close file
  file.close();
}

// https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c
bool endsWith(std::string const& fullString, std::string const& ending) {
  if (fullString.length() >= ending.length()) {
    return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
  } else {
    return false;
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    spdlog::critical("Usage: {} <input file> [<output file> [options]]", argv[0]);
    return 1;
  }

  // parameters
  std::string in_tpx3 = argv[1];
  std::string out_dat = (argc > 2) ? argv[2] : "";
  bool no_output = (argc > 2) ? false : true;

  // no options implies single-thread
  bool use_tbb = false;
  // mmap controls memory mapping or alloc+read
  bool use_mmap = false;
  bool use_tgdc = false;
  bool debug = false;
  float pulse_rate = 0.0;
  if (argc > 3) {
    use_tbb = strstr(argv[3], "tbb") != NULL;
    use_mmap = strstr(argv[3], "mmap") != NULL;
    use_tgdc = strstr(argv[3], "tgdc") != NULL;
    debug = strstr(argv[3], "debug") != NULL;

    // select pulse rate
    pulse_rate = strstr(argv[3], "1hz") ? 1.0 : pulse_rate;
    pulse_rate = strstr(argv[3], "10hz") ? 10.0 : pulse_rate;
    pulse_rate = strstr(argv[3], "15hz") ? 15.0 : pulse_rate;
    pulse_rate = strstr(argv[3], "30hz") ? 30.0 : pulse_rate;
    pulse_rate = strstr(argv[3], "45hz") ? 45.0 : pulse_rate;
    pulse_rate = strstr(argv[3], "60hz") ? 60.0 : pulse_rate;
  }

  if (debug) spdlog::set_level(spdlog::level::debug);

  // report statistics
  size_t n_hits = 0;
  int n_bad_hits = 0;
  size_t n_events = 0;

  // manage timers (for statistics)
  // but see also NOTES section of https://en.cppreference.com/w/cpp/chrono/high_resolution_clock
  enum { TOTAL = 0, RAW_DATA, BATCHES, EVENTS, GATHER, OUTPUT, NUM_TIMERS };
  struct {
    std::chrono::time_point<std::chrono::high_resolution_clock> begin;
    std::chrono::time_point<std::chrono::high_resolution_clock> end;
    double accumulated;
  } timer[NUM_TIMERS];
  for (auto& a : timer) {
    a.accumulated = 0.0;
  }

  // obtain pointer to data
  std::string method_raw_data = use_mmap ? "Mapping" : "Reading";
  spdlog::debug("{} input: {}", method_raw_data, in_tpx3);
  timer[TOTAL].begin = timer[RAW_DATA].begin = std::chrono::high_resolution_clock::now();
  auto raw_data = use_mmap ? mmapTPX3RawToMapInfo(in_tpx3) : readTPX3RawToMapInfo(in_tpx3);
  timer[RAW_DATA].end = std::chrono::high_resolution_clock::now();
  timer[RAW_DATA].accumulated = static_cast<double>(
      std::chrono::duration_cast<std::chrono::microseconds>(timer[RAW_DATA].end - timer[RAW_DATA].begin).count());

  size_t raw_data_consumed = 0;
  unsigned long tdc_timestamp = 0;
  unsigned long long int gdc_timestamp = 0;
  unsigned long timer_lsb32 = 0;

  // output data
  tbb::concurrent_vector<std::vector<Neutron>> events;
  std::string method_events;

  spdlog::debug("@{:p}, {}", raw_data.map, raw_data.max);

  if (raw_data.map == NULL) {
    spdlog::error("Insufficient memory: {}", in_tpx3);
    exit(EXIT_FAILURE);
  }

  // NB: processing large memory-mapped files requires a restriction
  // on the amount of raw data that is treated by the algorithm

  if (use_tbb) {
    spdlog::debug("Processing tbb...");
    method_events = "TBB Parallel";
  } else {
    spdlog::debug("Processing single-thread...");
    method_events = "Single-Thread";
  }

  std::cerr << std::endl;
  std::cerr.precision(2);

  while (raw_data_consumed < raw_data.max) {
    // manage partial batches
    size_t consumed = 0;
    char* raw_data_ptr = raw_data.map + raw_data_consumed;
    size_t raw_data_size = raw_data.max - raw_data_consumed;

    // spdlog::debug("raw_data: {}/{} ({:.2}%)", raw_data_consumed, raw_data.max,
    // static_cast<double>(raw_data_consumed)*100.0/static_cast<double>(raw_data.max));
    std::cerr << "\rraw_data: " << raw_data_consumed << "/" << raw_data.max << " ("
              << static_cast<double>(raw_data_consumed) * 100.0 / static_cast<double>(raw_data.max) << "%)";

    timer[BATCHES].begin = std::chrono::high_resolution_clock::now();
    auto batches = findTPX3H(raw_data_ptr, raw_data_size, consumed);
    if (use_tgdc) {
      // extract tdc and gdc timestamps from the batches read so far
      for (auto& tpx3 : batches) {
        updateTimestamp(tpx3, raw_data_ptr, consumed, tdc_timestamp, gdc_timestamp, timer_lsb32);
      }
    }
    timer[BATCHES].end = timer[EVENTS].begin = std::chrono::high_resolution_clock::now();

    if (use_tbb) {
      // https://github.com/jjallaire/TBB/blob/master/inst/examples/parallel-vector-sum.cpp
      struct ComputeEvents {
        // source vector(s)
        char* input;
        std::size_t range;
        std::vector<TPX3>& batch;

        // output vector-of-vector-of-events
        tbb::concurrent_vector<std::vector<Neutron>> output;

        // standard and splitting constructor
        ComputeEvents(char* input, std::size_t range, std::vector<TPX3>& batch)
            : input(input), range(range), batch(batch), output() {}
        ComputeEvents(ComputeEvents& body, tbb::split)
            : input(body.input), range(body.range), batch(body.batch), output() {}

        // generate just the range of hits
        void operator()(const tbb::blocked_range<size_t>& r) {
          auto abs_alg_mt = std::make_unique<ABS>(5.0, 1, 75);
          for (size_t i = r.begin(); i != r.end(); ++i) {
            TPX3& _tpx3 = batch[i];
            extractHits(_tpx3, input, range);
            abs_alg_mt->reset();
            abs_alg_mt->fit(_tpx3.hits);
            output.push_back(abs_alg_mt->get_events(_tpx3.hits));
          }
        }

        // join vectors
        void join(ComputeEvents& rhs) {
          for (auto& a : rhs.output) {
            output.push_back(a);
          }
        }
      };

      ComputeEvents compute(raw_data_ptr, consumed, batches);
      tbb::parallel_reduce(tbb::blocked_range<size_t>(0, batches.size()), compute);
      // transfer vectors
      for (auto& a : compute.output) {
        events.push_back(a);
      }
    } else {
      auto abs_alg = std::make_unique<ABS>(5.0, 1, 75);
      for (auto& _tpx3 : batches) {
        extractHits(_tpx3, raw_data_ptr, consumed);
        abs_alg->reset();
        abs_alg->fit(_tpx3.hits);
        events.push_back(abs_alg->get_events(_tpx3.hits));
      }
    }
    timer[EVENTS].end = std::chrono::high_resolution_clock::now();

    // report statistics
    for (const auto& tpx3 : batches) {
      n_hits += tpx3.hits.size();
    }
    // sanity check: hit.getTOF() should be smaller than 666,667 clock, which is
    //               equivalent to 16.67 ms
    if (pulse_rate > 0.0) {
      for (const auto& tpx3 : batches) {
        for (const auto& hit : tpx3.hits) {
          auto tof_ms = hit.getTOF_ns() * 1e-6;
          if (tof_ms > (1.0 / pulse_rate) + 1e-6) {
            spdlog::debug("TOF: {} ms", tof_ms);
            n_bad_hits++;
          }
        }
      }
    }
    for (const auto& e : events) {
      n_events += e.size();
    }

    // manage iterations on partial raw_data
    raw_data_consumed += consumed;
    timer[BATCHES].accumulated += static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(timer[BATCHES].end - timer[BATCHES].begin).count());
    timer[EVENTS].accumulated += static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(timer[EVENTS].end - timer[EVENTS].begin).count());
  }

  std::cerr << std::endl;

  timer[TOTAL].end = std::chrono::high_resolution_clock::now();
  timer[TOTAL].accumulated += static_cast<double>(
      std::chrono::duration_cast<std::chrono::microseconds>(timer[TOTAL].end - timer[TOTAL].begin).count());

  spdlog::info("Number of hits: {}", n_hits);
  spdlog::info("Number of events: {}", n_events);

  if (n_hits > 0) {
    spdlog::info("bad/total hit ratio: {:.2f}%", (n_bad_hits * 100.0) / n_hits);

    auto speed_raw_data = n_hits / (timer[RAW_DATA].accumulated / 1e6);
    auto speed_batches = n_hits / (timer[BATCHES].accumulated / 1e6);
    auto speed_events = n_hits / (timer[EVENTS].accumulated / 1e6);
    auto speed_aggregate = n_hits / (timer[TOTAL].accumulated / 1e6);
    spdlog::info("{:s} speed: {:<e} hits/s", method_raw_data, speed_raw_data);
    spdlog::info("{:s} speed: {:<e} hits/s", "Batching", speed_batches);
    spdlog::info("{:s} speed: {:<e} hits/s", method_events, speed_events);
    spdlog::info("{:s} speed: {:<e} hits/s", "Aggregate", speed_aggregate);
  }

  // write output
  // save events to file
  if (!no_output && n_events > 0) {
    spdlog::debug("Writing output: {}", out_dat);

    // NB: events is a vector-of-vectors-of-events that we transfrom into a vector-of-events
    timer[GATHER].begin = std::chrono::high_resolution_clock::now();
    std::vector<Neutron> v;
    for (auto& a : events) {
      // because Neutron doesn't have a copy constructor
      for (auto& e : a) {
        Neutron n(e.getX(), e.getY(), e.getTOF_ns(), e.getTOT(), e.getNHits());
        v.push_back(n);
      }
    }
    timer[GATHER].end = timer[OUTPUT].begin = std::chrono::high_resolution_clock::now();

    std::string csv_ext = ".csv";
    std::string bin_ext = ".bin";
    std::string h5_ext = ".h5";

    if (endsWith(out_dat, csv_ext))
      saveEventsToCSV(out_dat, v);
    else if (endsWith(out_dat, bin_ext))
      saveEventsToBIN(out_dat, v);
    else if (endsWith(out_dat, h5_ext))
      saveEventsToHDF5(out_dat, v);
    else {
      spdlog::debug("Unhanded extention (.bin, .csv or .h5 are known)");
      no_output = true;
    }
    timer[OUTPUT].end = std::chrono::high_resolution_clock::now();

    timer[GATHER].accumulated += static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(timer[GATHER].end - timer[GATHER].begin).count());
    timer[OUTPUT].accumulated += static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(timer[OUTPUT].end - timer[OUTPUT].begin).count());
    auto speed_gather = n_events / (timer[GATHER].accumulated / 1e6);
    auto speed_output = n_events / (timer[OUTPUT].accumulated / 1e6);
    spdlog::info("{:s} speed: {:<e} events/s", "Gathering Events", speed_gather);
    if (!no_output) spdlog::info("{:s} speed: {:<e} events/s", "Writing Output", speed_output);
  }
}
