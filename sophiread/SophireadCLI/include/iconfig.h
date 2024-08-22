/**
 * @file iconfig.h
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Config interface
 * @version 0.1
 * @date 2024-08-16
 *
 * @copyright Copyright (c) 2024
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