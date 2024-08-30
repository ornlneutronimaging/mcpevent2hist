/**
 * @file disk_io.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Contains utility functions for disk IO.
 * @version 0.1
 * @date 2023-09-01
 *
 * @copyright Copyright (c) 2023
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#include "disk_io.h"

#include <filesystem>

#include "spdlog/spdlog.h"

/**
 * @brief Read Timepix3 raw data from file into memory as a vector of char for
 * subsequent analysis.
 *
 * @param[in] tpx3file
 * @return std::vector<char>
 */
std::vector<char> readTPX3RawToCharVec(const std::string &tpx3file) {
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
 * @brief Read Timepix3 raw data from file into memory.
 *
 * @param tpx3file
 * @return mapinfo_t that defines { char *, and size_t }
 */
mapinfo_t readTPX3RawToMapInfo(const std::string &tpx3file) {
  mapinfo_t info = {-1, NULL, 0};

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
  info.max = static_cast<size_t>(fileSize);
  info.map = reinterpret_cast<char *>(malloc(info.max));

  // Read the data from file and store it in the vector
  if (info.map == NULL)
    info.max = 0;
  else
    file.read(info.map, info.max);

  // Close the file
  file.close();

  return info;
}

/**
 * @brief Memory-map a Timepix3 raw data file (without pre-reading it).
 *
 * @param tpx3file
 * @return mapinfo_t that defines { char *, and size_t }
 */
mapinfo_t mmapTPX3RawToMapInfo(const std::string &tpx3file) {
  mapinfo_t info = {-1, NULL, 0};

  std::ifstream file(tpx3file, std::ios::binary);
  if (!file.is_open()) {
    spdlog::error("Failed to open file: {}", tpx3file);
    exit(EXIT_FAILURE);
  }

  info.fd = open(tpx3file.c_str(), O_RDWR, 0666);

  if (info.fd == -1) {
    perror(tpx3file.c_str());
    exit(EXIT_FAILURE);
  }
  info.max = lseek(info.fd, 0, SEEK_END);  // determine the sizeof the file
  info.map = reinterpret_cast<char *>(
      mmap(0, info.max, PROT_READ | PROT_WRITE, MAP_SHARED, info.fd, 0));
  // https://lemire.me/blog/2012/06/26/which-is-fastest-read-fread-ifstream-or-mmap/
  // says to add MAP_POPULATE to make it mmap() faster...
  // TODO: Test this

  return info;
  // https://stackoverflow.com/questions/26569217/do-i-have-to-munmap-a-mmap-file
  // ( consensus is that you do not need to do it )
}

/**
 * @brief Append microsecond timestamp to the file name.
 *
 * @param[in] originalFileName
 * @return std::string
 */
std::string generateFileNameWithMicroTimestamp(
    const std::string &originalFileName) {
  auto now = std::chrono::high_resolution_clock::now();
  auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
  auto micros =
      std::chrono::duration_cast<std::chrono::microseconds>(now - seconds);

  std::filesystem::path filePath(originalFileName);
  std::string baseName = filePath.stem().string();
  std::string extension = filePath.extension().string();
  std::filesystem::path parentPath = filePath.parent_path();

  // use h5 as extension if not specified
  if (extension.empty()) {
    extension = ".h5";
  }

  std::stringstream newFileName;
  newFileName << baseName << "_" << std::setfill('0') << std::setw(6)
              << micros.count() << extension;

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
void writeDatasetToGroup(H5::Group &group, const std::string &dataset_name,
                         ForwardIterator begin, ForwardIterator end,
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
    const std::string &out_file_name, ForwardIterator data_begin,
    ForwardIterator data_end, const std::string &baseGroupName,
    const std::vector<
        std::pair<std::string, std::function<T(const decltype(*data_begin) &)>>>
        &attributes,
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

  std::string groupName =
      append ? generateGroupName(out_file, baseGroupName) : baseGroupName;
  H5::Group group = out_file.createGroup(groupName);

  for (const auto &[dataset_name, func] : attributes) {
    if constexpr (std::is_same_v<T, int>) {
      writeDatasetToGroup<int>(group, dataset_name, data_begin, data_end, func,
                               H5::IntType(H5::PredType::NATIVE_INT));
    } else if constexpr (std::is_same_v<T, double>) {
      writeDatasetToGroup<double>(group, dataset_name, data_begin, data_end,
                                  func,
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
void saveOrAppendHitsToHDF5(const std::string &out_file_name,
                            ForwardIterator hits_begin,
                            ForwardIterator hits_end, bool append) {
  saveOrAppendToHDF5<double>(
      out_file_name, hits_begin, hits_end, "hits",
      {
          {"x",
           [](const auto &hit) { return static_cast<double>(hit.getX()); }},
          {"y",
           [](const auto &hit) { return static_cast<double>(hit.getY()); }},
          {"tot_ns", [](const auto &hit) { return hit.getTOT_ns(); }},
          {"toa_ns", [](const auto &hit) { return hit.getTOA_ns(); }},
          {"ftoa_ns", [](const auto &hit) { return hit.getFTOA_ns(); }},
          {"tof_ns", [](const auto &hit) { return hit.getTOF_ns(); }},
          {"spidertime_ns",
           [](const auto &hit) { return hit.getSPIDERTIME_ns(); }},
      },
      append);
}

/**
 * @brief Save a hit vector to a HDF5 file. If the file already exists, rename
 the file with a microsecond timestamp as suffix.
 *
 * @tparam ForwardIterator
 * @param[in] out_file_name
 * @param[in] hits_begin
 * @param[in] hits_end
 */
template <typename ForwardIterator>
void saveHitsToHDF5(const std::string &out_file_name,
                    ForwardIterator hits_begin, ForwardIterator hits_end) {
  std::string finalFileName = out_file_name;
  if (std::filesystem::exists(out_file_name)) {
    spdlog::warn("File '{}' already exists. Renaming the output file.",
                 out_file_name);
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
void saveHitsToHDF5(const std::string &out_file_name,
                    const std::vector<Hit> &hits) {
  saveHitsToHDF5(out_file_name, hits.cbegin(), hits.cend());
}

/**
 * @brief Append a hit vector to a HDF5 file. If the file already exists, append
 * the hits to the existing file.
 *
 * @tparam ForwardIterator
 * @param[in] out_file_name
 * @param[in] hits_begin
 * @param[in] hits_end
 */
template <typename ForwardIterator>
void appendHitsToHDF5(const std::string &out_file_name,
                      ForwardIterator hits_begin, ForwardIterator hits_end) {
  saveOrAppendHitsToHDF5(out_file_name, hits_begin, hits_end, true);
}

/**
 * @brief Append hits to HDF5 file (wrapper function)
 *
 * @param[in] out_file_name
 * @param[in] hits
 */
void appendHitsToHDF5(const std::string &out_file_name,
                      const std::vector<Hit> &hits) {
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
void saveOrAppendNeutronToHDF5(const std::string &out_file_name,
                               ForwardIterator neutron_begin,
                               ForwardIterator neutron_end, bool append) {
  saveOrAppendToHDF5<double>(
      out_file_name, neutron_begin, neutron_end, "neutrons",
      {
          {"x", [](const Neutron &neutron) { return neutron.getX(); }},
          {"y", [](const Neutron &neutron) { return neutron.getY(); }},
          {"tof_ns",
           [](const Neutron &neutron) { return neutron.getTOF_ns(); }},
          {"tot_ns",
           [](const Neutron &neutron) { return neutron.getTOT_ns(); }},
          {"nHits", [](const Neutron &neutron) { return neutron.getNHits(); }},
      },
      append);
}

/**
 * @brief Save a neutron vector to a HDF5 file. If the file already exists,
 rename the file with a microsecond timestamp
 * as suffix.
 *
 * @tparam ForwardIterator
 * @param[in] out_file_name
 * @param[in] neutron_begin
 * @param[in] neutron_end
 */
template <typename ForwardIterator>
void saveNeutronToHDF5(const std::string &out_file_name,
                       ForwardIterator neutron_begin,
                       ForwardIterator neutron_end) {
  std::string finalFileName = out_file_name;
  if (std::filesystem::exists(out_file_name)) {
    spdlog::warn("File '{}' already exists. Renaming the output file.",
                 out_file_name);
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
void saveNeutronToHDF5(const std::string &out_file_name,
                       const std::vector<Neutron> &neutrons) {
  saveNeutronToHDF5(out_file_name, neutrons.cbegin(), neutrons.cend());
}

/**
 * @brief Append a neutron vector to a HDF5 file. If group "neutrons" already
 * exists, append the neutrons as a new group.
 *
 * @tparam ForwardIterator
 * @param[in] out_file_name
 * @param[in] neutron_begin
 * @param[in] neutron_end
 */
template <typename ForwardIterator>
void appendNeutronToHDF5(const std::string &out_file_name,
                         ForwardIterator neutron_begin,
                         ForwardIterator neutron_end) {
  saveOrAppendNeutronToHDF5(out_file_name, neutron_begin, neutron_end, true);
}

/**
 * @brief Append neutrons to HDF5 file (wrapper function)
 *
 * @param[in] out_file_name
 * @param[in] neutrons
 */
void appendNeutronToHDF5(const std::string &out_file_name,
                         const std::vector<Neutron> &neutrons) {
  appendNeutronToHDF5(out_file_name, neutrons.cbegin(), neutrons.cend());
}

TPX3FileReader::TPX3FileReader(const std::string& filename) {
    fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        spdlog::error("Failed to open file: {}", filename);
        throw std::runtime_error("Failed to open file");
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        spdlog::error("Failed to get file size");
        close(fd);
        throw std::runtime_error("Failed to get file size");
    }

    fileSize = sb.st_size;
    map = static_cast<char*>(mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0));
    if (map == MAP_FAILED) {
        spdlog::error("Failed to mmap file");
        close(fd);
        throw std::runtime_error("Failed to mmap file");
    }

    currentPosition = 0;
    spdlog::info("Opened file: {}, size: {} bytes", filename, fileSize);
}

TPX3FileReader::~TPX3FileReader() {
    if (map != MAP_FAILED) {
        munmap(map, fileSize);
    }
    if (fd != -1) {
        close(fd);
    }
}

std::vector<char> TPX3FileReader::readChunk(size_t chunkSize) {
    if (currentPosition >= fileSize) {
        return std::vector<char>();  // Return empty vector if we've reached the end of the file
    }

    size_t remainingBytes = fileSize - currentPosition;
    size_t bytesToRead = std::min(chunkSize, remainingBytes);

    std::vector<char> chunk(map + currentPosition, map + currentPosition + bytesToRead);
    currentPosition += bytesToRead;

    spdlog::debug("Read chunk of size {} bytes, current position: {}/{}", bytesToRead, currentPosition, fileSize);
    return chunk;
}

void createOrExtendDataset(H5::Group& group, const std::string& datasetName, 
                           const std::vector<double>& data) {
    hsize_t dims[1] = {data.size()};
    hsize_t maxdims[1] = {H5S_UNLIMITED};
    hsize_t chunkdims[1] = {std::min(static_cast<hsize_t>(1024), dims[0])};  // Adjust chunk size as needed

    H5::DataSpace dataspace;
    H5::DataSet dataset;

    try {
        // Try to open existing dataset
        dataset = group.openDataSet(datasetName);
        dataspace = dataset.getSpace();
        
        // Get current dimensions
        hsize_t currentDims[1];
        dataspace.getSimpleExtentDims(currentDims);

        // Extend the dataset
        hsize_t newDims[1] = {currentDims[0] + dims[0]};
        dataset.extend(newDims);

        // Select the newly added portion of the dataset
        dataspace = dataset.getSpace();
        hsize_t offset[1] = {currentDims[0]};
        dataspace.selectHyperslab(H5S_SELECT_SET, dims, offset);

    } catch (H5::Exception& e) {
        // Dataset doesn't exist, create a new one
        H5::DSetCreatPropList propList;
        propList.setChunk(1, chunkdims);

        dataspace = H5::DataSpace(1, dims, maxdims);
        dataset = group.createDataSet(datasetName, H5::PredType::NATIVE_DOUBLE, dataspace, propList);
    }

    // Write the data
    H5::DataSpace memspace(1, dims);
    dataset.write(data.data(), H5::PredType::NATIVE_DOUBLE, memspace, dataspace);
}

void appendHitsToHDF5Extendible(H5::H5File& file, const std::vector<Hit>& hits) {
    H5::Group group;
    try {
        group = file.openGroup("hits");
    } catch (H5::Exception& e) {
        group = file.createGroup("hits");
    }

    std::vector<double> xData, yData, totData, toaData, ftoaData, tofData, spidertimeData;
    xData.reserve(hits.size());
    yData.reserve(hits.size());
    totData.reserve(hits.size());
    toaData.reserve(hits.size());
    ftoaData.reserve(hits.size());
    tofData.reserve(hits.size());
    spidertimeData.reserve(hits.size());

    for (const auto& hit : hits) {
        xData.push_back(static_cast<double>(hit.getX()));
        yData.push_back(static_cast<double>(hit.getY()));
        totData.push_back(hit.getTOT_ns());
        toaData.push_back(hit.getTOA_ns());
        ftoaData.push_back(hit.getFTOA_ns());
        tofData.push_back(hit.getTOF_ns());
        spidertimeData.push_back(hit.getSPIDERTIME_ns());
    }

    createOrExtendDataset(group, "x", xData);
    createOrExtendDataset(group, "y", yData);
    createOrExtendDataset(group, "tot_ns", totData);
    createOrExtendDataset(group, "toa_ns", toaData);
    createOrExtendDataset(group, "ftoa_ns", ftoaData);
    createOrExtendDataset(group, "tof_ns", tofData);
    createOrExtendDataset(group, "spidertime_ns", spidertimeData);

    group.close();
}

void appendNeutronsToHDF5Extendible(H5::H5File& file, const std::vector<Neutron>& neutrons) {
    H5::Group group;
    try {
        group = file.openGroup("neutrons");
    } catch (H5::Exception& e) {
        group = file.createGroup("neutrons");
    }

    std::vector<double> xData, yData, tofData, totData, nHitsData;
    xData.reserve(neutrons.size());
    yData.reserve(neutrons.size());
    tofData.reserve(neutrons.size());
    totData.reserve(neutrons.size());
    nHitsData.reserve(neutrons.size());

    for (const auto& neutron : neutrons) {
        xData.push_back(neutron.getX());
        yData.push_back(neutron.getY());
        tofData.push_back(neutron.getTOF_ns());
        totData.push_back(neutron.getTOT_ns());
        nHitsData.push_back(static_cast<double>(neutron.getNHits()));
    }

    createOrExtendDataset(group, "x", xData);
    createOrExtendDataset(group, "y", yData);
    createOrExtendDataset(group, "tof_ns", tofData);
    createOrExtendDataset(group, "tot_ns", totData);
    createOrExtendDataset(group, "nHits", nHitsData);

    group.close();
}