/**
 * @file disk_io.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Contains utility functions for disk IO.
 * @version 0.1
 * @date 2023-09-01
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
#pragma once

#include <H5Cpp.h>

#include <fstream>
#include <iostream>
#include <vector>

#include "hit.h"
#include "neutron.h"

std::vector<char> readTPX3RawToCharVec(const std::string& tpx3file);

std::string generateFileNameWithMicroTimestamp(const std::string& originalFileName);

template <typename ForwardIterator>
void saveHitsToHDF5(const std::string& out_file_path, ForwardIterator hits_begin, ForwardIterator hits_end);

void saveHitsToHDF5(const std::string& out_file_path, const std::vector<Hit>& hits);

void saveNeutronToHDF5(const std::string out_file_path, const std::vector<Neutron>& neutrons);