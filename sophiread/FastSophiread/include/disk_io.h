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
#include <functional>
#include <iostream>
#include <vector>

#include "hit.h"
#include "neutron.h"

std::vector<char> readTPX3RawToCharVec(const std::string& tpx3file);

typedef struct mapinfo {
    int fd;
    char *map;
    size_t max;
} mapinfo_t;

mapinfo_t readTPX3RawToMapInfo(const std::string& tpx3file);
mapinfo_t mmapTPX3RawToMapInfo(const std::string& tpx3file);

std::string generateFileNameWithMicroTimestamp(const std::string& originalFileName);

template <typename T, typename ForwardIterator>
void writeDatasetToGroup(H5::Group& group, const std::string& dataset_name, ForwardIterator begin, ForwardIterator end,
                         const H5::DataType& data_type);

std::string generateGroupName(H5::H5File& file, const std::string& baseName);

template <typename T, typename ForwardIterator>
void saveOrAppendToHDF5(
    const std::string& out_file_name, ForwardIterator data_begin, ForwardIterator data_end,
    const std::string& baseGroupName,
    const std::vector<std::pair<std::string, std::function<T(const decltype(*data_begin)&)>>>& attributes, bool append);

template <typename ForwardIterator>
void saveOrAppendHitsToHDF5(const std::string& out_file_name, ForwardIterator hits_begin, ForwardIterator hits_end,
                            bool appendMode = false);
template <typename ForwardIterator>
void saveHitsToHDF5(const std::string& out_file_path, ForwardIterator hits_begin, ForwardIterator hits_end);
void saveHitsToHDF5(const std::string& out_file_path, const std::vector<Hit>& hits);
template <typename ForwardIterator>
void appendHitsToHDF5(const std::string& out_file_name, ForwardIterator hits_begin, ForwardIterator hits_end);
void appendHitsToHDF5(const std::string& out_file_name, const std::vector<Hit>& hits);

template <typename ForwardIterator>
void saveOrAppendNeutronToHDF5(const std::string& out_file_name, ForwardIterator neutron_begin,
                               ForwardIterator neutron_end, bool append = false);
template <typename ForwardIterator>
void saveNeutronToHDF5(const std::string& out_file_name, ForwardIterator neutron_begin, ForwardIterator neutron_end);
void saveNeutronToHDF5(const std::string& out_file_name, const std::vector<Neutron>& neutrons);
template <typename ForwardIterator>
void appendNeutronToHDF5(const std::string& out_file_name, ForwardIterator neutron_begin, ForwardIterator neutron_end);
void appendNeutronToHDF5(const std::string& out_file_name, const std::vector<Neutron>& neutrons);
