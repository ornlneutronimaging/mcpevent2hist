/**
 * @file sophiread_core.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Core functions for CLI application
 * @version 0.1
 * @date 2024-08-21
 *
 * @copyright Copyright (c) 2024
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#pragma once

#include <string>
#include <vector>

#include "abs.h"
#include "iconfig.h"
#include "tpx3_fast.h"

namespace sophiread {

std::vector<char> timedReadDataToCharVec(const std::string& in_tpx3);
std::vector<TPX3> timedFindTPX3H(const std::vector<char>& rawdata);
void timedLocateTimeStamp(std::vector<TPX3>& batches,
                          const std::vector<char>& chunk,
                          unsigned long& tdc_timestamp,
                          unsigned long long& gdc_timestamp,
                          unsigned long& timer_lsb32);
void timedProcessing(std::vector<TPX3>& batches,
                     const std::vector<char>& raw_data, const IConfig& config);
void timedSaveHitsToHDF5(const std::string& out_hits,
                         std::vector<TPX3>& batches);
void timedSaveEventsToHDF5(const std::string& out_events,
                           std::vector<TPX3>& batches);
std::vector<std::vector<std::vector<unsigned int>>> timedCreateTOFImages(
    const std::vector<TPX3>& batches, double super_resolution,
    const std::vector<double>& tof_bin_edges, const std::string& mode);
void timedSaveTOFImagingToTIFF(
    const std::string& out_tof_imaging,
    const std::vector<std::vector<std::vector<unsigned int>>>& tof_images,
    const std::vector<double>& tof_bin_edges,
    const std::string& tof_filename_base);
std::vector<std::vector<std::vector<unsigned int>>> initializeTOFImages(
    double super_resolution, const std::vector<double>& tof_bin_edges);
void updateTOFImages(
    std::vector<std::vector<std::vector<unsigned int>>>& tof_images,
    const TPX3& batch, double super_resolution,
    const std::vector<double>& tof_bin_edges, const std::string& mode);
std::vector<uint64_t> calculateSpectralCounts(
    const std::vector<std::vector<std::vector<unsigned int>>>& tof_images);
void writeSpectralFile(const std::string& filename,
                       const std::vector<uint64_t>& spectral_counts,
                       const std::vector<double>& tof_bin_edges);
}  // namespace sophiread
