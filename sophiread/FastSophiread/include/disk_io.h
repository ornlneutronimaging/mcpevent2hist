/**
 * @file disk_io.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Contains utility functions for disk IO.
 * @version 0.1
 * @date 2023-09-01
 *
 * @copyright Copyright (c) 2023
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#pragma once

#include <H5Cpp.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <functional>
#include <iostream>
#include <vector>

#include "hit.h"
#include "neutron.h"

std::vector<char> readTPX3RawToCharVec(const std::string& tpx3file);

typedef struct mapinfo {
  int fd;
  char* map;
  size_t max;
} mapinfo_t;

mapinfo_t readTPX3RawToMapInfo(const std::string& tpx3file);
mapinfo_t mmapTPX3RawToMapInfo(const std::string& tpx3file);

std::string generateFileNameWithMicroTimestamp(
    const std::string& originalFileName);

template <typename T, typename ForwardIterator>
void writeDatasetToGroup(H5::Group& group, const std::string& dataset_name,
                         ForwardIterator begin, ForwardIterator end,
                         const H5::DataType& data_type);

std::string generateGroupName(H5::H5File& file, const std::string& baseName);

template <typename T, typename ForwardIterator>
void saveOrAppendToHDF5(
    const std::string& out_file_name, ForwardIterator data_begin,
    ForwardIterator data_end, const std::string& baseGroupName,
    const std::vector<
        std::pair<std::string, std::function<T(const decltype(*data_begin)&)>>>&
        attributes,
    bool append);

template <typename ForwardIterator>
void saveOrAppendHitsToHDF5(const std::string& out_file_name,
                            ForwardIterator hits_begin,
                            ForwardIterator hits_end, bool appendMode = false);
template <typename ForwardIterator>
void saveHitsToHDF5(const std::string& out_file_path,
                    ForwardIterator hits_begin, ForwardIterator hits_end);
void saveHitsToHDF5(const std::string& out_file_path,
                    const std::vector<Hit>& hits);
template <typename ForwardIterator>
void appendHitsToHDF5(const std::string& out_file_name,
                      ForwardIterator hits_begin, ForwardIterator hits_end);
void appendHitsToHDF5(const std::string& out_file_name,
                      const std::vector<Hit>& hits);

template <typename ForwardIterator>
void saveOrAppendNeutronToHDF5(const std::string& out_file_name,
                               ForwardIterator neutron_begin,
                               ForwardIterator neutron_end,
                               bool append = false);
template <typename ForwardIterator>
void saveNeutronToHDF5(const std::string& out_file_name,
                       ForwardIterator neutron_begin,
                       ForwardIterator neutron_end);
void saveNeutronToHDF5(const std::string& out_file_name,
                       const std::vector<Neutron>& neutrons);
template <typename ForwardIterator>
void appendNeutronToHDF5(const std::string& out_file_name,
                         ForwardIterator neutron_begin,
                         ForwardIterator neutron_end);
void appendNeutronToHDF5(const std::string& out_file_name,
                         const std::vector<Neutron>& neutrons);

class TPX3FileReader {
 public:
  TPX3FileReader(const std::string& filename);
  ~TPX3FileReader();

  std::vector<char> readChunk(size_t chunkSize);
  bool isEOF() const { return currentPosition >= fileSize; }
  size_t getTotalSize() const { return fileSize; }

 private:
  int fd;
  char* map;
  size_t fileSize;
  size_t currentPosition;
};

// Helper functions to append data to extendible datasets
void createOrExtendDataset(H5::Group& group, const std::string& datasetName,
                           const std::vector<double>& data);
void appendHitsToHDF5Extendible(H5::H5File& file, const std::vector<Hit>& hits);
void appendNeutronsToHDF5Extendible(H5::H5File& file,
                                    const std::vector<Neutron>& neutrons);
