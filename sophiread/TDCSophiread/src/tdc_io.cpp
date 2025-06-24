// TDCSophiread Memory-Mapped I/O Implementation
// SPDX-License-Identifier: GPL-3.0+

#include "tdc_io.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>
#include <stdexcept>

namespace tdcsophiread {

std::unique_ptr<MappedFile> MappedFile::open(const std::string& filepath) {
  // Check if file exists
  if (!std::filesystem::exists(filepath)) {
    throw std::runtime_error("File does not exist: " + filepath);
  }

  // Get file size
  std::error_code ec;
  auto file_size = std::filesystem::file_size(filepath, ec);
  if (ec) {
    throw std::runtime_error("Cannot determine file size: " + filepath + " (" +
                             ec.message() + ")");
  }

  // Handle empty files
  if (file_size == 0) {
    // Return a valid MappedFile with null data for empty files
    return std::unique_ptr<MappedFile>(
        new MappedFile(filepath, nullptr, 0, -1));
  }

  // Open file with read-only access
  int fd = ::open(filepath.c_str(), O_RDONLY);
  if (fd == -1) {
    throw std::runtime_error("Cannot open file: " + filepath);
  }

  // Memory map the file
  void* mapped_data = ::mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapped_data == MAP_FAILED) {
    ::close(fd);
    throw std::runtime_error("Cannot memory map file: " + filepath);
  }

  // Advise kernel about access pattern (sequential read for TPX3 processing)
  ::madvise(mapped_data, file_size, MADV_SEQUENTIAL);

  return std::unique_ptr<MappedFile>(new MappedFile(
      filepath, static_cast<const uint8_t*>(mapped_data), file_size, fd));
}

MappedFile::MappedFile(const std::string& filepath, const uint8_t* data,
                       size_t size, int fd)
    : m_Filepath(filepath), m_Data(data), m_Size(size), m_FileDescriptor(fd) {}

MappedFile::~MappedFile() {
  // Unmap memory if it was mapped
  if (m_Data != nullptr && m_Size > 0) {
    ::munmap(const_cast<uint8_t*>(m_Data), m_Size);
  }

  // Close file descriptor if valid
  if (m_FileDescriptor != -1) {
    ::close(m_FileDescriptor);
  }
}

}  // namespace tdcsophiread