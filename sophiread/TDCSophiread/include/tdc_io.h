// TDCSophiread Memory-Mapped I/O
// Memory-mapped file reader for TPX3 data (Linux & macOS only)
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace tdcsophiread {

/**
 * @brief Memory-mapped file reader for TPX3 binary data
 *
 * Provides efficient read-only access to large TPX3 files using
 * memory mapping. Only supports Linux and macOS platforms.
 */
class MappedFile {
 public:
  /**
   * @brief Factory method to open and map a file
   *
   * @param filepath Path to the file to map
   * @param offset Byte offset to start mapping (default: 0)
   * @param size Number of bytes to map (default: 0 = entire file)
   * @return std::unique_ptr<MappedFile> Mapped file instance
   * @throws std::runtime_error if file cannot be opened or mapped
   */
  static std::unique_ptr<MappedFile> open(const std::string& filepath,
                                          size_t offset = 0, size_t size = 0);

  /**
   * @brief Destructor - unmaps the file
   */
  ~MappedFile();

  // Non-copyable but movable
  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;
  MappedFile(MappedFile&&) = default;
  MappedFile& operator=(MappedFile&&) = default;

  /**
   * @brief Get pointer to mapped data
   *
   * @return const uint8_t* Read-only pointer to file data
   */
  const uint8_t* data() const { return m_Data; }

  /**
   * @brief Get size of mapped region
   *
   * @return size_t Mapped region size in bytes
   */
  size_t size() const { return m_Size; }

  /**
   * @brief Get total file size
   *
   * @return size_t Total file size in bytes
   */
  size_t file_size() const { return m_FileSize; }

  /**
   * @brief Get file path
   *
   * @return const std::string& Original file path
   */
  const std::string& path() const { return m_Filepath; }

 private:
  /**
   * @brief Private constructor - use open() factory method
   *
   * @param filepath File path
   * @param data Mapped data pointer (adjusted for offset)
   * @param size Mapped region size
   * @param file_size Total file size
   * @param fd File descriptor (Unix)
   * @param mapped_ptr Actual mmap pointer (for unmapping)
   * @param mapped_size Actual mmap size (for unmapping)
   */
  MappedFile(const std::string& filepath, const uint8_t* data, size_t size,
             size_t file_size, int fd, void* mapped_ptr, size_t mapped_size);

  std::string m_Filepath;  ///< Original file path
  const uint8_t* m_Data;   ///< Mapped data pointer (adjusted for offset)
  size_t m_Size;           ///< Mapped region size in bytes
  size_t m_FileSize;       ///< Total file size in bytes
  int m_FileDescriptor;    ///< Unix file descriptor
  void* m_MappedPtr;       ///< Actual mmap pointer (for unmapping)
  size_t m_MappedSize;     ///< Actual mmap size (for unmapping)
};

}  // namespace tdcsophiread