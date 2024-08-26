/**
 * @file iposition_tof.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Interface for neutron and hit
 * @version 0.1
 * @date 2024-08-20
 *
 * @copyright Copyright (c) 2024
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#pragma once

class IPositionTOF {
 public:
  virtual ~IPositionTOF() = default;
  virtual double iGetX() const = 0;
  virtual double iGetY() const = 0;
  virtual double iGetTOF_ns() const = 0;
};
