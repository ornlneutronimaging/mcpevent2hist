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
   * @return std::unique_ptr<MappedFile> Mapped file instance
   * @throws std::runtime_error if file cannot be opened or mapped
   */
  static std::unique_ptr<MappedFile> open(const std::string& filepath);

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
   * @brief Get size of mapped file
   *
   * @return size_t File size in bytes
   */
  size_t size() const { return m_Size; }

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
   * @param data Mapped data pointer
   * @param size File size
   * @param fd File descriptor (Unix)
   */
  MappedFile(const std::string& filepath, const uint8_t* data, size_t size,
             int fd);

  std::string m_Filepath;  ///< Original file path
  const uint8_t* m_Data;   ///< Mapped data pointer
  size_t m_Size;           ///< File size in bytes
  int m_FileDescriptor;    ///< Unix file descriptor
};

}  // namespace tdcsophiread