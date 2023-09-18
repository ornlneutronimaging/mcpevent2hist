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
 * @brief Save hits to HDF5 file.
 *
 * @tparam ForwardIterator
 * @param[in] out_file_name
 * @param[in] hits_begin
 * @param[in] hits_end
 */
template <typename ForwardIterator>
void saveHitsToHDF5(const std::string &out_file_name, ForwardIterator hits_begin, ForwardIterator hits_end) {
  // Determine the number of hits
  const size_t num_hits = std::distance(hits_begin, hits_end);

  // Sanity check
  if (num_hits == 0) {
    spdlog::warn("No hits to save. Exiting function.");
    return;  // Gracefully exit
  }

  // Decide file name
  std::string finalFileName = out_file_name;
  if (std::filesystem::exists(out_file_name)) {
    spdlog::warn("File '{}' already exists. Renaming the output file.", out_file_name);
    finalFileName = generateFileNameWithMicroTimestamp(out_file_name);
    spdlog::info("New output file: '{}'", finalFileName);
  }
  // Use finalFileName instead of out_file_name throughout the rest of the function
  H5::H5File out_file(finalFileName, H5F_ACC_TRUNC);

  // Preparation
  hsize_t dims[1] = {num_hits};

  H5::DataSpace dataspace(1, dims);
  H5::IntType int_type(H5::PredType::NATIVE_INT);
  H5::FloatType float_type(H5::PredType::NATIVE_DOUBLE);

  // Make hits as a group
  H5::Group group = out_file.createGroup("hits");

  // Write x
  std::vector<int> x(num_hits);
  std::transform(hits_begin, hits_end, x.begin(), [](const auto &hit) { return hit.getX(); });
  H5::DataSet x_dataset = group.createDataSet("x", int_type, dataspace);
  x_dataset.write(x.data(), int_type);
  x_dataset.close();

  // Write y
  std::vector<int> y(num_hits);
  std::transform(hits_begin, hits_end, y.begin(), [](const auto &hit) { return hit.getY(); });
  H5::DataSet y_dataset = group.createDataSet("y", int_type, dataspace);
  y_dataset.write(y.data(), int_type);
  y_dataset.close();

  // Write tot_ns
  std::vector<double> tot_ns(num_hits);
  std::transform(hits_begin, hits_end, tot_ns.begin(), [](const auto &hit) { return hit.getTOT_ns(); });
  H5::DataSet tot_ns_dataset = group.createDataSet("tot_ns", float_type, dataspace);
  tot_ns_dataset.write(tot_ns.data(), float_type);
  tot_ns_dataset.close();

  // Write toa_ns
  std::vector<double> toa_ns(num_hits);
  std::transform(hits_begin, hits_end, toa_ns.begin(), [](const auto &hit) { return hit.getTOA_ns(); });
  H5::DataSet toa_ns_dataset = group.createDataSet("toa_ns", float_type, dataspace);
  toa_ns_dataset.write(toa_ns.data(), float_type);
  toa_ns_dataset.close();

  // Write ftoa_ns
  std::vector<double> ftoa_ns(num_hits);
  std::transform(hits_begin, hits_end, ftoa_ns.begin(), [](const auto &hit) { return hit.getFTOA_ns(); });
  H5::DataSet ftoa_ns_dataset = group.createDataSet("ftoa_ns", float_type, dataspace);
  ftoa_ns_dataset.write(ftoa_ns.data(), float_type);
  ftoa_ns_dataset.close();

  // Write tof_ns
  std::vector<double> tof_ns(num_hits);
  std::transform(hits_begin, hits_end, tof_ns.begin(), [](const auto &hit) { return hit.getTOF_ns(); });
  H5::DataSet tof_ns_dataset = group.createDataSet("tof_ns", float_type, dataspace);
  tof_ns_dataset.write(tof_ns.data(), float_type);
  tof_ns_dataset.close();

  // Write spidertime_ns
  std::vector<double> spidertime_ns(num_hits);
  std::transform(hits_begin, hits_end, spidertime_ns.begin(), [](const auto &hit) { return hit.getSPIDERTIME_ns(); });
  H5::DataSet spidertime_ns_dataset = group.createDataSet("spidertime_ns", float_type, dataspace);
  spidertime_ns_dataset.write(spidertime_ns.data(), float_type);
  spidertime_ns_dataset.close();

  group.close();

  // Close file
  out_file.close();
}

/**
 * @brief Save hits to HDF5 file.
 *
 * @param[in] out_file_name
 * @param[in] hits
 */
void saveHitsToHDF5(const std::string &out_file_name, const std::vector<Hit> &hits) {
  saveHitsToHDF5(out_file_name, hits.cbegin(), hits.cend());
}