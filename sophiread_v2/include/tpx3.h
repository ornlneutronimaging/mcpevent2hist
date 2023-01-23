#include <fstream>
#include <vector>

/**
 * @brief Class to store the data of a single hit of a charged particle 
 *        hitting the timepix3 sensor.
 * 
 */
class Hit {
    public:
        Hit(
            const int x,
            const int y,
            const int tot,
            const int toa,
            const int ftoa,
            const unsigned int tof,
            const int spidertime);

        int getX() const {return m_x;};
        int getY() const {return m_y;};
        int getTOT() const {return m_tot;};
        int getTOA() const {return m_toa;};
        int getFTOA() const {return m_ftoa;};
        int getSPIDERTIME() const {return m_spidertime;};
        unsigned int getTOF() const {return m_tof;};

        double getTOF_ns() const {return m_tof * m_scale_to_ns;};
        double getTOA_ns() const {return m_toa * m_scale_to_ns;};
        double getTOT_ns() const {return m_tot * m_scale_to_ns;};

    private:
        // raw packet directly read from tpx3.
        const int m_x, m_y;  // pixel coordinates
        const int m_tot;  // time over threshold
        const int m_toa;  // time of arrival (40MHz clock, 14 bit)
        const int m_ftoa;  // fine time of arrival (640MHz clock, 4 bit)
        const unsigned int m_tof;
        const int m_spidertime; // time from the spider board

        // scale factor that converts time to ns
        // NOTE: 40 MHz clock is used for the timepix3, hence 25 ns per tick.
        double m_scale_to_ns = 25.0;
};

std::vector<Hit> readTimepix3RawData(const std::string& filepath);
