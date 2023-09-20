/**
 * @file disk_io.cpp
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
#include "disk_io.h"

#include <filesystem>

#include "spdlog/spdlog.h"

/**
 * @brief Read Timepix3 raw data from file into memory as a vector of char for subsequent analysis.
 *
 * @param[in] tpx3file
 * @return std::vector<char>
 */
std::vector<char> readTPX3RawToCharVec(const std::string& tpx3file) {
  // Open the file
  std::ifstream file(tpx3file, std::ios::binary | std::ios::ate);

  // Check if file is open successfully
  if (!file.is_open()) {
    spdlog::error("Failed to open file: {}", tpx3file);
    exit(EXIT_FAILURE);
  }

  // Get the size of the file
  std::streampos fileSize = file.tellg();
  file.seekg(0, std::ios::beg);
  spdlog::info("File size: {} bytes", static_cast<size_t>(fileSize));

  // Allocate a vector to store the data
  std::vector<char> vec(fileSize);

  // Read the data from file and store it in the vector
  file.read(vec.data(), fileSize);

  // Close the file
  file.close();

  return vec;
}

/**
 * @brief Append microsecond timestamp to the file name.
 *
 * @param[in] originalFileName
 * @return std::string
 */
std::string generateFileNameWithMicroTimestamp(const std::string& originalFileName) {
  auto now = std::chrono::high_resolution_clock::now();
  auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
  auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now - seconds);

  std::filesystem::path filePath(originalFileName);
  std::string baseName = filePath.stem().string();
  std::string extension = filePath.extension().string();
  std::filesystem::path parentPath = filePath.parent_path();

  // use h5 as extention if not specified
  if (extension.empty()) {
    extension = ".h5";
  }

  std::stringstream newFileName;
  newFileName << baseName << "_" << std::setfill('0') << std::setw(6) << micros.count() << extension;

  if (!parentPath.empty()) {
    return (parentPath / newFileName.str()).string();
  } else {
    return newFileName.str();
  }
}

/**
 * @brief Helper function to write a dataset to the given group.
 *
 * @tparam T
 * @tparam ForwardIterator
 * @tparam Func
 * @param[in, out] group
 * @param[in] dataset_name
 * @param[in] begin
 * @param[in] end
 * @param[in] transform_func
 * @param[in] data_type
 */
template <typename T, typename ForwardIterator, typename Func>
void writeDatasetToGroup(H5::Group &group, const std::string &dataset_name, ForwardIterator begin, ForwardIterator end,
                         Func transform_func, const H5::DataType &data_type) {
  std::vector<T> data;
  data.reserve(std::distance(begin, end));
  std::transform(begin, end, std::back_inserter(data), transform_func);
  hsize_t dims[1] = {data.size()};
  H5::DataSpace dataspace(1, dims);
  H5::DataSet dataset = group.createDataSet(dataset_name, data_type, dataspace);
  dataset.write(data.data(), data_type);
  dataset.close();
}

/**
 * @brief Generate a group name that has a suffix of "_n" where n is an integer.
 *
 * @param[in] file
 * @param[in] baseName
 * @return std::string
 */
std::string generateGroupName(H5::H5File &file, const std::string &baseName) {
  std::string groupName = baseName;
  int counter = 1;
  while (H5Lexists(file.getId(), groupName.c_str(), H5P_DEFAULT)) {
    groupName = baseName + "_" + std::to_string(counter);
    counter++;
  }
  return groupName;
}

/**
 * @brief Universal function to save or append data to a HDF5 file.
 *
 * @tparam T
 * @tparam ForwardIterator
 * @param[in] out_file_name
 * @param[in] data_begin
 * @param[in] data_end
 * @param[in] baseGroupName
 * @param[in] attributes
 * @param[in] append
 */
template <typename T, typename ForwardIterator>
void saveOrAppendToHDF5(
    const std::string &out_file_name, ForwardIterator data_begin, ForwardIterator data_end,
    const std::string &baseGroupName,
    const std::vector<std::pair<std::string, std::function<T(const decltype(*data_begin) &)>>> &attributes,
    bool append) {
  const size_t num_data = std::distance(data_begin, data_end);

  if (num_data == 0) {
    spdlog::warn("No data to process. Exiting function.");
    return;
  }

  H5::H5File out_file;
  if (!std::filesystem::exists(out_file_name) || !append) {
    out_file = H5::H5File(out_file_name, H5F_ACC_TRUNC);
  } else {
    out_file = H5::H5File(out_file_name, H5F_ACC_RDWR);
  }

  std::string groupName = append ? generateGroupName(out_file, baseGroupName) : baseGroupName;
  H5::Group group = out_file.createGroup(groupName);

  for (const auto &[dataset_name, func] : attributes) {
    if constexpr (std::is_same_v<T, int>) {
      writeDatasetToGroup<int>(group, dataset_name, data_begin, data_end, func, H5::IntType(H5::PredType::NATIVE_INT));
    } else if constexpr (std::is_same_v<T, double>) {
      writeDatasetToGroup<double>(group, dataset_name, data_begin, data_end, func,
                                  H5::FloatType(H5::PredType::NATIVE_DOUBLE));
    }
  }

  group.close();
  out_file.close();
}

/**
 * @brief Specialized function to save or append hits to a HDF5 file.
 *
 * @tparam ForwardIterator
 * @param[in] out_file_name
 * @param[in] hits_begin
 * @param[in] hits_end
 * @param[in] append
 */
template <typename ForwardIterator>
void saveOrAppendHitsToHDF5(const std::string &out_file_name, ForwardIterator hits_begin, ForwardIterator hits_end,
                            bool append) {
  saveOrAppendToHDF5<int>(out_file_name, hits_begin, hits_end, "hits",
                          {
                              {"x", [](const auto &hit) { return hit.getX(); }},
                              {"y", [](const auto &hit) { return hit.getY(); }},
                              {"tot_ns", [](const auto &hit) { return hit.getTOT_ns(); }},
                              {"toa_ns", [](const auto &hit) { return hit.getTOA_ns(); }},
                              {"ftoa_ns", [](const auto &hit) { return hit.getFTOA_ns(); }},
                              {"tof_ns", [](const auto &hit) { return hit.getTOF_ns(); }},
                              {"spidertime_ns", [](const auto &hit) { return hit.getSPIDERTIME_ns(); }},
                          },
                          append);
}

/**
 * @brief Save a hit vector to a HDF5 file. If the file already exists, rename the file with a microsecond timestamp
 as suffix.
 *
 * @tparam ForwardIterator
 * @param[in] out_file_name
 * @param[in] hits_begin
 * @param[in] hits_end
 */
template <typename ForwardIterator>
void saveHitsToHDF5(const std::string &out_file_name, ForwardIterator hits_begin, ForwardIterator hits_end) {
  std::string finalFileName = out_file_name;
  if (std::filesystem::exists(out_file_name)) {
    spdlog::warn("File '{}' already exists. Renaming the output file.", out_file_name);
    finalFileName = generateFileNameWithMicroTimestamp(out_file_name);
    spdlog::info("New output file: '{}'", finalFileName);
  }
  saveOrAppendHitsToHDF5(finalFileName, hits_begin, hits_end, false);
}

/**
 * @brief Save hits to HDF5 file (wrapper function)
 *
 * @param[in] out_file_name
 * @param[in] hits
 */
void saveHitsToHDF5(const std::string &out_file_name, const std::vector<Hit> &hits) {
  saveHitsToHDF5(out_file_name, hits.cbegin(), hits.cend());
}

/**
 * @brief Append a hit vector to a HDF5 file. If the file already exists, append the hits to the existing file.
 *
 * @tparam ForwardIterator
 * @param[in] out_file_name
 * @param[in] hits_begin
 * @param[in] hits_end
 */
template <typename ForwardIterator>
void appendHitsToHDF5(const std::string &out_file_name, ForwardIterator hits_begin, ForwardIterator hits_end) {
  saveOrAppendHitsToHDF5(out_file_name, hits_begin, hits_end, true);
}

/**
 * @brief Append hits to HDF5 file (wrapper function)
 *
 * @param[in] out_file_name
 * @param[in] hits
 */
void appendHitsToHDF5(const std::string &out_file_name, const std::vector<Hit> &hits) {
  appendHitsToHDF5(out_file_name, hits.cbegin(), hits.cend());
}

/**
 * @brief Specialized function to save or append neutrons to a HDF5 file.
 *
 * @tparam ForwardIterator
 * @param[in] out_file_name
 * @param[in] neutron_begin
 * @param[in] neutron_end
 * @param[in] append
 */
template <typename ForwardIterator>
void saveOrAppendNeutronToHDF5(const std::string &out_file_name, ForwardIterator neutron_begin,
                               ForwardIterator neutron_end, bool append) {
  saveOrAppendToHDF5<double>(out_file_name, neutron_begin, neutron_end, "neutrons",
                             {
                                 {"x", [](const Neutron &neutron) { return neutron.getX(); }},
                                 {"y", [](const Neutron &neutron) { return neutron.getY(); }},
                                 {"tof", [](const Neutron &neutron) { return neutron.getTOF(); }},
                                 {"tot", [](const Neutron &neutron) { return neutron.getTOT(); }},
                                 {"nHits", [](const Neutron &neutron) { return neutron.getNHits(); }},
                             },
                             append);
}

/**
 * @brief Save a neutron vector to a HDF5 file. If the file already exists, rename the file with a microsecond
 timestamp
 * as suffix.
 *
 * @tparam ForwardIterator
 * @param[in] out_file_name
 * @param[in] neutron_begin
 * @param[in] neutron_end
 */
template <typename ForwardIterator>
void saveNeutronToHDF5(const std::string &out_file_name, ForwardIterator neutron_begin, ForwardIterator neutron_end) {
  std::string finalFileName = out_file_name;
  if (std::filesystem::exists(out_file_name)) {
    spdlog::warn("File '{}' already exists. Renaming the output file.", out_file_name);
    finalFileName = generateFileNameWithMicroTimestamp(out_file_name);
    spdlog::info("New output file: '{}'", finalFileName);
  }
  saveOrAppendNeutronToHDF5(finalFileName, neutron_begin, neutron_end, false);
}

/**
 * @brief Save neutrons to HDF5 file (wrapper function)
 *
 * @param[in] out_file_name
 * @param[in] neutrons
 */
void saveNeutronToHDF5(const std::string &out_file_name, const std::vector<Neutron> &neutrons) {
  saveNeutronToHDF5(out_file_name, neutrons.cbegin(), neutrons.cend());
}

/**
 * @brief Append a neutron vector to a HDF5 file. If group "neutrons" already exists, append the neutrons as a new
 * group.
 *
 * @tparam ForwardIterator
 * @param[in] out_file_name
 * @param[in] neutron_begin
 * @param[in] neutron_end
 */
template <typename ForwardIterator>
void appendNeutronToHDF5(const std::string &out_file_name, ForwardIterator neutron_begin, ForwardIterator neutron_end) {
  saveOrAppendNeutronToHDF5(out_file_name, neutron_begin, neutron_end, true);
}

/**
 * @brief Append neutrons to HDF5 file (wrapper function)
 *
 * @param[in] out_file_name
 * @param[in] neutrons
 */
void appendNeutronToHDF5(const std::string &out_file_name, const std::vector<Neutron> &neutrons) {
  appendNeutronToHDF5(out_file_name, neutrons.cbegin(), neutrons.cend());
}