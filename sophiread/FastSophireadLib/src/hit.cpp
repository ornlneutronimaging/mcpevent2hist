/**
 * @file hit.cpp
 * @author Chen Zhang (zhangc@orn.gov)
 * @brief Implementation of Hit class
 * @version 0.1
 * @date 2023-09-04
 *
 * @copyright Copyright (c) 2023
 * BSD 3-Clause License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of ORNL nor the names of its contributors may be used
 * to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "hit.h"

/**
 * @brief Special constructor that construct a Hit from raw bytes.
 *
 * @param packet
 * @param tdc
 * @param gdc
 * @param chip_layout_type
 */
Hit::Hit(const char *packet, const unsigned long long tdc, const unsigned long long gdc, const int chip_layout_type) {
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

  // rename variables for clarity
  unsigned long long int GDC_timestamp = gdc;
  unsigned long long TDC_timestamp = tdc;

  // convert spidertime to global timestamp
  unsigned long SPDR_LSB30 = 0;
  unsigned long SPDR_MSB18 = 0;
  //   unsigned long long SPDR_timestamp = 0;

  SPDR_LSB30 = GDC_timestamp & 0x3FFFFFFF;
  SPDR_MSB18 = (GDC_timestamp >> 30) & 0x3FFFF;
  if (spidertime < SPDR_LSB30) {
    SPDR_MSB18++;
  }
  m_spidertime = (SPDR_MSB18 << 30) & 0xFFFFC0000000;
  m_spidertime = m_spidertime | spidertime;

  // tof calculation
  // TDC packets not always arrive before corresponding data packets
  if (m_spidertime < TDC_timestamp) {
    m_tof = m_spidertime - TDC_timestamp + 16666667;  // 1E9 / 60.0 is approximately 16666667
  } else {
    m_tof = m_spidertime - TDC_timestamp;
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
