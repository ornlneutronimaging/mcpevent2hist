/**
 * @file tiff_types.h
 * @author Chen Zhang (zhangc@ornl.gov)
 * @brief Define TIFF bit depth
 * @version 0.1
 * @date 2024-08-23
 *
 * @copyright Copyright (c) 2024
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#pragma once

#include <cstdint>

// Define bit-depth types for TIFF images
using TIFF8Bit = uint8_t;      // 8-bit
using TIFF16Bit = uint16_t;    // 16-bit
using TIFF32Bit = uint32_t;    // 32-bit