#include "tpx3.h"

Hit::Hit(const int x, const int y, const int tot, const int toa, const int ftoa, const unsigned int tof) :
    m_x(x), m_y(y), m_tot(tot), m_toa(toa), m_ftoa(ftoa), m_tof(tof) {}

/**
 * @brief Get the X coordinate of the hit.
 * 
 * @return int 
 */
int Hit::getX() const {
    return m_x;
}

/**
 * @brief Get the Y coordinate of the hit.
 * 
 * @return int 
 */
int Hit::getY() const {
    return m_y;
}

/**
 * @brief Get the TOT of the hit.
 * 
 * @return int 
 */
int Hit::getTOT() const {
    return m_tot;
}

/**
 * @brief Get the TOA of the hit.
 * 
 * @return int 
 */
int Hit::getTOA() const {
    return m_toa;
}

/**
 * @brief Get the TOF of the hit.
 * 
 * @return unsigned int 
 */
unsigned int Hit::getTOF() const {
    return m_tof;
}

/**
 * @brief Get the TOF of the hit in ns.
 * 
 * @return const double 
 */
double Hit::getTOF_ns() const {
    return m_tof * m_scale_to_ns;
}

/**
 * @brief Get the TOA of the hit in ns.
 * 
 * @return const double 
 */
double Hit::getTOA_ns() const {
    return m_toa * m_scale_to_ns;
}

/**
 * @brief Get the TOT of the hit in ns.
 * 
 * @return const double 
 */
double Hit::getTOT_ns() const {
    return m_tot * m_scale_to_ns;
}
