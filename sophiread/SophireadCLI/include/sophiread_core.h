/**
 * @file sophiread_core.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Core functions for CLI application
 * @version 0.1
 * @date 2024-08-21
 *
 * @copyright Copyright (c) 2024
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
#pragma once

#include <string>
#include <vector>

#include "abs.h"
#include "iconfig.h"
#include "tpx3_fast.h"

namespace sophiread {

std::vector<char> timedReadDataToCharVec(const std::string& in_tpx3);
std::vector<TPX3> timedFindTPX3H(const std::vector<char>& rawdata);
void timedLocateTimeStamp(std::vector<TPX3>& batches, const std::vector<char>& rawdata);
void timedProcessing(std::vector<TPX3>& batches, const std::vector<char>& raw_data, const IConfig& config);
void timedSaveHitsToHDF5(const std::string& out_hits, std::vector<TPX3>& batches);
void timedSaveEventsToHDF5(const std::string& out_events, std::vector<TPX3>& batches);
std::vector<std::vector<std::vector<unsigned int>>> timedCreateTOFImages(const std::vector<TPX3>& batches,
                                                                         double super_resolution,
                                                                         const std::vector<double>& tof_bin_edges,
                                                                         const std::string& mode);
void timedSaveTOFImagingToTIFF(const std::string& out_tof_imaging,
                               const std::vector<std::vector<std::vector<unsigned int>>>& tof_images,
                               const std::vector<double>& tof_bin_edges, const std::string& tof_filename_base);
}  // namespace sophiread
