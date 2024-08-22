/**
 * @file hit.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @author Su-Ann Chong (chongs@ornl.gov)
 * @brief Implementation of Hit class
 * @version 0.1
 * @date 2023-09-04
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
#include "hit.h"

#include <iostream>

/**
 * @brief Special constructor that construct a Hit from raw bytes.
 *
 * @param[in] packet
 * @param[in] tdc
 * @param[in] gdc
 * @param[in] chip_layout_type
 */
Hit::Hit(const char *packet, const unsigned long long TDC_timestamp, const unsigned long long GDC_timestamp,
         const int chip_layout_type) {
  // local variables
  unsigned short pixaddr, dcol, spix, pix;
  unsigned short *spider_time;
  unsigned short *nTOT;    // bytes 2,3, raw time over threshold
  unsigned int *nTOA;      // bytes 3,4,5,6, raw time of arrival
  unsigned int *npixaddr;  // bytes 4,5,6,7
  unsigned int spidertime = 0;

  // timing information
  spider_time = (unsigned short *)(&packet[0]);  // Spider time  (16 bits)
  nTOT = (unsigned short *)(&packet[2]);         // ToT          (10 bits)
  nTOA = (unsigned int *)(&packet[3]);           // ToA          (14 bits)
  m_ftoa = *nTOT & 0xF;                          // fine ToA     (4 bits)
  m_tot = (*nTOT >> 4) & 0x3FF;
  m_toa = (*nTOA >> 6) & 0x3FFF;
  spidertime = 16384 * (*spider_time) + m_toa;

  // convert spidertime to global timestamp
  unsigned long SPDR_LSB30 = 0;
  unsigned long SPDR_MSB18 = 0;

  SPDR_LSB30 = GDC_timestamp & 0x3FFFFFFF;
  SPDR_MSB18 = (GDC_timestamp >> 30) & 0x3FFFF;
  if (spidertime < SPDR_LSB30) {
    SPDR_MSB18++;
  }
  m_spidertime = (SPDR_MSB18 << 30) & 0xFFFFC0000000;
  m_spidertime = m_spidertime | spidertime;

  // additional check to make sure rollover of spidertime is correct
  // 4e7 is roughly 1 second in the units of 25 ns
  // 1073741824 is 2^30 (in units of 25 ns)
  if ((m_spidertime - GDC_timestamp) >= 4e7) {
    m_spidertime -= 1073741824;
  }

  // tof calculation
  m_tof = m_spidertime - TDC_timestamp;
  while (m_tof * 25E-6 > 16.67) {
    m_tof -= 666667;
  }

  // pixel address
  npixaddr = (unsigned int *)(&packet[4]);  // Pixel address (14 bits)
  pixaddr = (*npixaddr >> 12) & 0xFFFF;
  dcol = ((pixaddr & 0xFE00) >> 8);
  spix = ((pixaddr & 0x1F8) >> 1);
  pix = pixaddr & 0x7;
  m_x = dcol + (pix >> 2);   // x coordinate
  m_y = spix + (pix & 0x3);  // y coordinate
  // adjustment for chip layout
  if (chip_layout_type == 0) {  // single
    m_x += 260;
    // m_y = m_y;
  } else if (chip_layout_type == 1) {  // double
    m_x = 255 - m_x + 260;
    m_y = 255 - m_y + 260;
  } else if (chip_layout_type == 2) {  // triple
    m_x = 255 - m_x;
    m_y = 255 - m_y + 260;
  }
}
