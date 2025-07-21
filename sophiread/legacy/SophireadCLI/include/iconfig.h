/**
 * @file iconfig.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Config interface
 * @version 0.1
 * @date 2024-08-16
 *
 * @copyright Copyright (c) 2024
 * SPDX - License - Identifier: GPL - 3.0 +
 */
#pragma once

#include <string>
#include <vector>

class IConfig {
 public:
  virtual ~IConfig() = default;

  virtual double getABSRadius() const = 0;
  virtual unsigned long int getABSMinClusterSize() const = 0;
  virtual unsigned long int getABSSpiderTimeRange() const = 0;
  virtual std::vector<double> getTOFBinEdges() const = 0;
  virtual double getSuperResolution() const = 0;

  virtual std::string toString() const = 0;
};