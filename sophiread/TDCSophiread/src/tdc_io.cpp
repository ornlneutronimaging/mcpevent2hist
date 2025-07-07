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

std::unique_ptr<MappedFile> MappedFile::open(const std::string& filepath,
                                             size_t offset, size_t size) {
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
        new MappedFile(filepath, nullptr, 0, 0, -1, nullptr, 0));
  }

  // Validate offset
  if (offset >= file_size) {
    throw std::runtime_error("Offset exceeds file size");
  }

  // Determine mapping size (default: from offset to end of file)
  size_t map_size = size;
  if (map_size == 0 || offset + map_size > file_size) {
    map_size = file_size - offset;
  }

  // Open file with read-only access
  int fd = ::open(filepath.c_str(), O_RDONLY);
  if (fd == -1) {
    throw std::runtime_error("Cannot open file: " + filepath);
  }

  // Align offset to page boundary for mmap
  size_t page_size = sysconf(_SC_PAGE_SIZE);
  size_t aligned_offset = (offset / page_size) * page_size;
  size_t offset_diff = offset - aligned_offset;
  size_t aligned_size = map_size + offset_diff;

  // Memory map the file chunk
  void* mapped_data =
      ::mmap(nullptr, aligned_size, PROT_READ, MAP_PRIVATE, fd, aligned_offset);
  if (mapped_data == MAP_FAILED) {
    ::close(fd);
    throw std::runtime_error("Cannot memory map file: " + filepath);
  }

  // Advise kernel about access pattern (sequential read for TPX3 processing)
  ::madvise(mapped_data, aligned_size, MADV_SEQUENTIAL);

  // Adjust pointer to requested offset
  const uint8_t* data_ptr =
      static_cast<const uint8_t*>(mapped_data) + offset_diff;

  return std::unique_ptr<MappedFile>(new MappedFile(
      filepath, data_ptr, map_size, file_size, fd, mapped_data, aligned_size));
}

MappedFile::MappedFile(const std::string& filepath, const uint8_t* data,
                       size_t size, size_t file_size, int fd, void* mapped_ptr,
                       size_t mapped_size)
    : m_Filepath(filepath),
      m_Data(data),
      m_Size(size),
      m_FileSize(file_size),
      m_FileDescriptor(fd),
      m_MappedPtr(mapped_ptr),
      m_MappedSize(mapped_size) {}

MappedFile::~MappedFile() {
  // Unmap memory if it was mapped (use actual mmap pointer and size)
  if (m_MappedPtr != nullptr && m_MappedSize > 0) {
    ::munmap(m_MappedPtr, m_MappedSize);
  }

  // Close file descriptor if valid
  if (m_FileDescriptor != -1) {
    ::close(m_FileDescriptor);
  }
}

}  // namespace tdcsophiread