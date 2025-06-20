/**
 * @file sophiread_core.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief CLI for reading Timepix3 raw data and parse it into neutron event
 * files and a tiff image (for visual inspection).
 * @date 2024-08-21
 *
 * @copyright Copyright (c) 2024
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#include "sophiread_core.h"

#include <spdlog/spdlog.h>
#include <tbb/tbb.h>
#include <tiffio.h>

#include <chrono>
#include <cmath>  // For std::isnan, std::isinf
#include <filesystem>
#include <fstream>
#include <numeric>  // For std::accumulate

#include "disk_io.h"
#include "tiff_types.h"

namespace sophiread {

/**
 * @brief Timed read raw data to char vector.
 *
 * @param[in] in_tpx3
 * @return std::vector<char>
 */
std::vector<char> timedReadDataToCharVec(const std::string &in_tpx3) {
  auto start = std::chrono::high_resolution_clock::now();
  auto raw_data = readTPX3RawToCharVec(in_tpx3);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();
  spdlog::info("Read raw data: {} s", elapsed / 1e6);

  return raw_data;
}

/**
 * @brief Timed find TPX3H.
 *
 * @param[in] chunk
 * @return std::vector<TPX3>
 */
std::vector<TPX3> timedFindTPX3H(const std::vector<char> &chunk) {
  auto start = std::chrono::high_resolution_clock::now();
  auto batches = findTPX3H(chunk);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();
  spdlog::info("Locate headers in chunk: {} s", elapsed / 1e6);

  return batches;
}

/**
 * @brief Timed locate timestamps.
 *
 * @param[in, out] batches
 * @param[in] chunk
 * @param[in, out] tdc_timestamp
 * @param[in, out] gdc_timestamp
 * @param[in, out] timer_lsb32
 */
void timedLocateTimeStamp(std::vector<TPX3> &batches,
                          const std::vector<char> &chunk,
                          unsigned long &tdc_timestamp,
                          unsigned long long &gdc_timestamp,
                          unsigned long &timer_lsb32) {
  auto start = std::chrono::high_resolution_clock::now();
  for (auto &tpx3 : batches) {
    updateTimestamp(tpx3, chunk, tdc_timestamp, gdc_timestamp, timer_lsb32);
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();
  spdlog::debug("Locate timestamps in chunk: {} s", elapsed / 1e6);
}

/**
 * @brief Timed locate timestamps without using GDC.
 *
 * This overloaded version processes timestamps using only TDC values,
 * useful for data streams where GDC information is not available or not needed.
 *
 * @param[in, out] batches The TPX3 batches to process
 * @param[in] chunk The raw data chunk
 * @param[in, out] tdc_timestamp The TDC timestamp to update
 */
void timedLocateTimeStamp(std::vector<TPX3> &batches,
                          const std::vector<char> &chunk,
                          unsigned long &tdc_timestamp) {
  auto start = std::chrono::high_resolution_clock::now();
  for (auto &tpx3 : batches) {
    updateTimestamp(tpx3, chunk, tdc_timestamp);
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();
  spdlog::debug("Locate timestamps (TDC only) in chunk: {} s", elapsed / 1e6);
}

/**
 * @brief Timed hits extraction and clustering via multi-threading.
 *
 * @param[in, out] batches
 * @param[in] chunk
 * @param[in] config
 */
void timedProcessing(std::vector<TPX3> &batches, const std::vector<char> &chunk,
                     const IConfig &config, bool useGDC) {
  auto start = std::chrono::high_resolution_clock::now();
  // GDC route
  if (useGDC) {
    spdlog::info("Using GDC mode for processing");
    tbb::parallel_for(tbb::blocked_range<size_t>(0, batches.size()),
                      [&](const tbb::blocked_range<size_t> &r) {
                        // Define ABS algorithm with user-defined parameters for
                        // each thread
                        auto abs_alg_mt = std::make_unique<ABS>(
                            config.getABSRadius(),
                            config.getABSMinClusterSize(),
                            config.getABSSpiderTimeRange());

                        for (size_t i = r.begin(); i != r.end(); ++i) {
                          auto &tpx3 = batches[i];
                          extractHits(tpx3, chunk);

                          abs_alg_mt->reset();
                          abs_alg_mt->set_method("centroid");
                          abs_alg_mt->fit(tpx3.hits);

                          tpx3.neutrons = abs_alg_mt->get_events(tpx3.hits);
                        }
                      });
  } else {
    spdlog::info("Using TDC mode for processing");
    tbb::parallel_for(tbb::blocked_range<size_t>(0, batches.size()),
                      [&](const tbb::blocked_range<size_t> &r) {
                        // Define ABS algorithm with user-defined parameters for
                        // each thread
                        auto abs_alg_mt = std::make_unique<ABS>(
                            config.getABSRadius(),
                            config.getABSMinClusterSize(),
                            config.getABSSpiderTimeRange());

                        for (size_t i = r.begin(); i != r.end(); ++i) {
                          auto &tpx3 = batches[i];
                          extractHitsTDC(tpx3, chunk);

                          abs_alg_mt->reset();
                          abs_alg_mt->set_method("centroid");
                          abs_alg_mt->fit(tpx3.hits);

                          tpx3.neutrons = abs_alg_mt->get_events(tpx3.hits);
                        }
                      });
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();
  spdlog::info("Process all hits -> neutrons: {} s", elapsed / 1e6);
}

/**
 * @brief Timed save hits to HDF5.
 *
 * @param[in] out_hits
 * @param[in] hits
 */
void timedSaveHitsToHDF5(const std::string &out_hits,
                         std::vector<TPX3> &batches) {
  auto start = std::chrono::high_resolution_clock::now();
  // move all hits into a single vector
  std::vector<Hit> hits;
  for (const auto &tpx3 : batches) {
    const auto &tpx3_hits = tpx3.hits;
    hits.insert(hits.end(), tpx3_hits.cbegin(), tpx3_hits.cend());
  }
  // save hits to HDF5 file
  saveHitsToHDF5(out_hits, hits);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();
  spdlog::info("Save hits to HDF5: {} s", elapsed / 1e6);
}

/**
 * @brief Timed save events to HDF5.
 *
 * @param[in] out_events
 * @param[in] batches
 */
void timedSaveEventsToHDF5(const std::string &out_events,
                           std::vector<TPX3> &batches) {
  auto start = std::chrono::high_resolution_clock::now();
  // move all events into a single vector
  std::vector<Neutron> events;
  for (const auto &tpx3 : batches) {
    const auto &tpx3_events = tpx3.neutrons;
    events.insert(events.end(), tpx3_events.cbegin(), tpx3_events.cend());
  }
  // save events to HDF5 file
  saveNeutronToHDF5(out_events, events);
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();
  spdlog::info("Save events to HDF5: {} s", elapsed / 1e6);
}

std::vector<std::vector<std::vector<unsigned int>>> initializeTOFImages(
    double super_resolution, const std::vector<double> &tof_bin_edges) {
  int dim_x = static_cast<int>(517 * super_resolution);
  int dim_y = static_cast<int>(517 * super_resolution);
  std::vector<std::vector<std::vector<unsigned int>>> tof_images(
      tof_bin_edges.size() - 1);
  for (auto &tof_image : tof_images) {
    tof_image.resize(dim_y, std::vector<unsigned int>(dim_x, 0));
  }
  return tof_images;
}

void updateTOFImages(
    std::vector<std::vector<std::vector<unsigned int>>> &tof_images,
    const TPX3 &batch, double super_resolution,
    const std::vector<double> &tof_bin_edges, const std::string &mode) {
  // Safety check for empty or invalid inputs
  if (tof_images.empty() || tof_bin_edges.size() < 2) {
    spdlog::error("Invalid TOF images or bin edges");
    return;
  }

  int dim_x = static_cast<int>(517 * super_resolution);
  int dim_y = static_cast<int>(517 * super_resolution);

  const std::vector<const IPositionTOF *> entries = [&]() {
    if (mode == "hit") {
      std::vector<const IPositionTOF *> result;
      result.reserve(batch.hits.size());
      for (const auto &hit : batch.hits) {
        result.push_back(static_cast<const IPositionTOF *>(&hit));
      }
      return result;
    } else {
      std::vector<const IPositionTOF *> result;
      result.reserve(batch.neutrons.size());
      for (const auto &neutron : batch.neutrons) {
        result.push_back(static_cast<const IPositionTOF *>(&neutron));
      }
      return result;
    }
  }();

  for (const auto &entry : entries) {
    try {
      // Safety check for null entry
      if (!entry) {
        continue;
      }

      double tof_ns = entry->iGetTOF_ns();

      // Safety check for invalid TOF values
      if (tof_ns < 0 || std::isnan(tof_ns) || std::isinf(tof_ns)) {
        spdlog::debug("Skipping entry with invalid TOF: {}", tof_ns);
        continue;
      }

      double tof_s = tof_ns / 1e9;

      // Safety check to make sure tof_s is in range
      if (tof_s < tof_bin_edges.front() || tof_s >= tof_bin_edges.back()) {
        spdlog::debug("TOF out of bin range: {}", tof_s);
        continue;
      }

      auto it =
          std::lower_bound(tof_bin_edges.begin(), tof_bin_edges.end(), tof_s);
      if (it != tof_bin_edges.begin()) {
        size_t bin_index = std::distance(tof_bin_edges.begin(), it) - 1;

        // Safety check for bin index
        if (bin_index >= tof_images.size()) {
          spdlog::debug("Bin index out of range: {}", bin_index);
          continue;
        }

        // Check and clamp x,y values to valid ranges
        double raw_x = entry->iGetX();
        double raw_y = entry->iGetY();

        // Skip invalid coordinates
        if (std::isnan(raw_x) || std::isnan(raw_y) || std::isinf(raw_x) ||
            std::isinf(raw_y)) {
          continue;
        }

        int x = std::round(raw_x * super_resolution);
        int y = std::round(raw_y * super_resolution);

        if (x >= 0 && x < dim_x && y >= 0 && y < dim_y) {
          tof_images[bin_index][y][x]++;
        }
      }
    } catch (const std::exception &e) {
      spdlog::error("Exception in updateTOFImages: {}", e.what());
    }
  }
}

/**
 * @brief Timed create TOF images.
 *
 * This function creates time-of-flight (TOF) images based on the provided
 * batches of TPX3 data. The TOF images are 2D histograms that represent the
 * distribution of neutron events in space for different TOF bins. The function
 * takes into account the super resolution, TOF bin edges, and the mode of
 * operation (hit or neutron) to generate the TOF images.
 *
 * @param[in] batches The vector of TPX3 batches containing the neutron events.
 * @param[in] super_resolution The super resolution factor used to calculate the
 * dimensions of each 2D histogram.
 * @param[in] tof_bin_edges The vector of TOF bin edges used to determine the
 * TOF bin for each neutron event.
 * @param[in] mode The mode of operation, either "hit" or "neutron", which
 * determines the type of events to process.
 * @return std::vector<std::vector<std::vector<unsigned int>>> The vector of TOF
 * images, where each TOF image is a 2D histogram representing the distribution
 * of neutron events in space for a specific TOF bin.
 */
std::vector<std::vector<std::vector<unsigned int>>> timedCreateTOFImages(
    const std::vector<TPX3> &batches, double super_resolution,
    const std::vector<double> &tof_bin_edges, const std::string &mode) {
  auto start = std::chrono::high_resolution_clock::now();

  // Initialize the TOF images container
  std::vector<std::vector<std::vector<unsigned int>>> tof_images(
      tof_bin_edges.size() - 1);

  // Sanity checks
  if (tof_bin_edges.size() < 2) {
    spdlog::error("Invalid TOF bin edges: at least 2 edges are required");
    return {};
  }
  if (batches.empty()) {
    spdlog::error("No batches to process");
    return tof_images;
  }

  // Calculate the dimensions of each 2D histogram based on super_resolution
  // one chip: 0-255 pixel pos
  // gap: 5
  // total: 0-255 + 5 + 0-255 -> 517 (TPX3@VENUS only)
  int dim_x = static_cast<int>(517 * super_resolution);
  int dim_y = static_cast<int>(517 * super_resolution);

  spdlog::debug("Creating TOF images with dimensions: {} x {}", dim_x, dim_y);
  spdlog::debug("tof_bin_edges size: {}", tof_bin_edges.size());
  if (!tof_bin_edges.empty()) {
    spdlog::debug("First bin edge: {}, Last bin edge: {}",
                  tof_bin_edges.front(), tof_bin_edges.back());
  }

  // Initialize each TOF bin's 2D histogram
  for (auto &tof_image : tof_images) {
    tof_image.resize(dim_y, std::vector<unsigned int>(dim_x, 0));
  }

  // Process neutrons from all batches
  size_t total_entries = 0;
  size_t binned_entries = 0;

  for (size_t batch_index = 0; batch_index < batches.size(); ++batch_index) {
    const auto &batch = batches[batch_index];
    spdlog::debug("Processing batch {}", batch_index);

    std::vector<const IPositionTOF *> entries;
    if (mode == "hit") {
      entries.reserve(batch.hits.size());
      for (const auto &hit : batch.hits) {
        entries.push_back(static_cast<const IPositionTOF *>(&hit));
      }
    } else {
      entries.reserve(batch.neutrons.size());
      for (const auto &neutron : batch.neutrons) {
        entries.push_back(static_cast<const IPositionTOF *>(&neutron));
      }
    }

    if (entries.empty()) {
      spdlog::debug("Batch {} is empty", batch_index);
      continue;
    }

    for (const auto &entry : entries) {
      try {
        // Safety check for null entry
        if (!entry) {
          continue;
        }

        total_entries++;
        const double tof_ns = entry->iGetTOF_ns();

        // Safety check for invalid TOF values
        if (tof_ns < 0 || std::isnan(tof_ns) || std::isinf(tof_ns)) {
          spdlog::debug("Skipping entry with invalid TOF: {}", tof_ns);
          continue;
        }

        const double tof_s = tof_ns / 1e9;

        // Safety check to make sure tof_s is in range
        if (tof_s < tof_bin_edges.front() || tof_s >= tof_bin_edges.back()) {
          spdlog::debug("TOF out of bin range: {}", tof_s);
          continue;
        }

        // Find the correct TOF bin
        // NOTE: tof_bin_edges are in sec, and tof_ns are in nano secs
        spdlog::debug("tof_ns: {}, tof_ns/1e9: {}", tof_ns, tof_s);

        if (const auto it = std::lower_bound(tof_bin_edges.cbegin(),
                                             tof_bin_edges.cend(), tof_s);
            it != tof_bin_edges.cbegin()) {
          const size_t bin_index =
              std::distance(tof_bin_edges.cbegin(), it) - 1;

          // Safety check for bin index
          if (bin_index >= tof_images.size()) {
            spdlog::debug("Bin index out of range: {}", bin_index);
            continue;
          }

          // Calculate the x and y indices in the 2D histogram
          double raw_x = entry->iGetX();
          double raw_y = entry->iGetY();

          // Skip invalid coordinates
          if (std::isnan(raw_x) || std::isnan(raw_y) || std::isinf(raw_x) ||
              std::isinf(raw_y)) {
            continue;
          }

          const int x = std::round(raw_x * super_resolution);
          const int y = std::round(raw_y * super_resolution);

          // Ensure x and y are within bounds
          if (x >= 0 && x < dim_x && y >= 0 && y < dim_y) {
            // Increment the count in the appropriate bin and position
            tof_images[bin_index][y][x]++;
            binned_entries++;
          }
        }
      } catch (const std::exception &e) {
        spdlog::error("Exception in timedCreateTOFImages: {}", e.what());
      }
    }
  }

  const auto end = std::chrono::high_resolution_clock::now();
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();
  spdlog::info("TOF image creation time: {} s", elapsed / 1e6);
  spdlog::info("Total entries: {}, Binned entries: {}", total_entries,
               binned_entries);

  return tof_images;
}

/**
 * @brief Timed save TOF imaging to TIFF.
 *
 * @param[in] out_tof_imaging
 * @param[in] batches
 * @param[in] tof_bin_edges
 * @param[in] tof_filename_base
 */
void timedSaveTOFImagingToTIFF(
    const std::string &out_tof_imaging,
    const std::vector<std::vector<std::vector<TIFF32Bit>>> &tof_images,
    const std::vector<double> &tof_bin_edges,
    const std::string &tof_filename_base) {
  auto start = std::chrono::high_resolution_clock::now();

  // 1. Create output directory if it doesn't exist
  if (!std::filesystem::exists(out_tof_imaging)) {
    std::filesystem::create_directories(out_tof_imaging);
    spdlog::info("Created output directory: {}", out_tof_imaging);
  }

  // 2. Iterate through each TOF bin and save TIFF files
  tbb::parallel_for(
      tbb::blocked_range<size_t>(0, tof_images.size()),
      [&](const tbb::blocked_range<size_t> &range) {
        for (size_t bin = range.begin(); bin < range.end(); ++bin) {
          // Construct filename
          std::string filename =
              fmt::format("{}/{}_bin_{:04d}.tiff", out_tof_imaging,
                          tof_filename_base, bin + 1);

          // prepare container and fill with current hist2d
          const uint32_t width = tof_images[bin][0].size();
          const uint32_t height = tof_images[bin].size();
          std::vector<std::vector<TIFF32Bit>> accumulated_image =
              tof_images[bin];

          // check if file already exist
          if (std::filesystem::exists(filename)) {
            TIFF *existing_tif = TIFFOpen(filename.c_str(), "r");
            if (existing_tif) {
              uint32_t existing_width, existing_height;
              TIFFGetField(existing_tif, TIFFTAG_IMAGEWIDTH, &existing_width);
              TIFFGetField(existing_tif, TIFFTAG_IMAGELENGTH, &existing_height);

              if (existing_width == width && existing_height == height) {
                // Dimensions match, proceed with accumulation
                for (uint32_t row = 0; row < height; ++row) {
                  std::vector<TIFF32Bit> scanline(width);
                  TIFFReadScanline(existing_tif, scanline.data(), row);
                  for (uint32_t col = 0; col < width; ++col) {
                    accumulated_image[row][col] += scanline[col];
                  }
                }
                spdlog::debug("Accumulated counts for existing file: {}",
                              filename);
              } else {
                spdlog::error(
                    "Dimension mismatch for file: {}. Expected {}x{}, got "
                    "{}x{}. Overwriting.",
                    filename, width, height, existing_width, existing_height);
              }
              TIFFClose(existing_tif);
            } else {
              spdlog::error("Failed to open existing TIFF file for reading: {}",
                            filename);
            }
          }

          // Write or update TIFF file
          TIFF *tif = TIFFOpen(filename.c_str(), "w");
          if (tif) {
            const uint32_t width = tof_images[bin][0].size();
            const uint32_t height = tof_images[bin].size();

            TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
            TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
            TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
            TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 32);
            TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
            TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
            TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);

            for (uint32_t row = 0; row < height; ++row) {
              TIFFWriteScanline(tif, accumulated_image[row].data(), row);
            }

            TIFFClose(tif);
            spdlog::debug("Wrote TIFF file: {}", filename);
          } else {
            spdlog::error("Failed to open TIFF file for writing: {}", filename);
          }
        }
      });

  // 3. Calculate spectral counts
  std::vector<uint64_t> spectral_counts = calculateSpectralCounts(tof_images);
  // 4. Write spectral file
  std::string spectral_filename =
      fmt::format("{}/{}_Spectra.txt", out_tof_imaging, tof_filename_base);
  writeSpectralFile(spectral_filename, spectral_counts, tof_bin_edges);

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  spdlog::info("TIFF and spectra file writing completed in {} ms",
               duration.count());
}

std::vector<uint64_t> calculateSpectralCounts(
    const std::vector<std::vector<std::vector<unsigned int>>> &tof_images) {
  std::vector<uint64_t> spectral_counts(tof_images.size(), 0);

  for (size_t bin = 0; bin < tof_images.size(); ++bin) {
    spectral_counts[bin] = std::accumulate(
        tof_images[bin].cbegin(), tof_images[bin].cend(),
        static_cast<uint64_t>(0), [](const auto sum, const auto &row) {
          return sum + std::accumulate(row.cbegin(), row.cend(), 0ULL);
        });
  }

  return spectral_counts;
}

void writeSpectralFile(const std::string &filename,
                       const std::vector<uint64_t> &spectral_counts,
                       const std::vector<double> &tof_bin_edges) {
  std::ofstream spectral_file(filename);
  if (spectral_file.is_open()) {
    spectral_file << "shutter_time,counts\n";
    for (size_t bin = 0; bin < tof_bin_edges.size() - 1; ++bin) {
      spectral_file << tof_bin_edges[bin + 1] << "," << spectral_counts[bin]
                    << "\n";
    }
    spectral_file.close();
    spdlog::info("Wrote spectral file: {}", filename);
  } else {
    spdlog::error("Failed to open spectra file for writing: {}", filename);
  }
}

}  // namespace sophiread