/*******************************************************************************
 * Copyright 2012 Bryan Godbolt, Hasitha Senanayake, Aakash Vasudevan
 *
 * This file is part of ANCL Autopilot.
 *
 *     ANCL Autopilot is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     ANCL Autopilot is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with ANCL Autopilot.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#ifndef RADIOCALIBRATION_H
#define RADIOCALIBRATION_H

/* STL Headers */
#include <string>
#include <mutex>

/* Project Headers */
#include "heli.h"
#include "Debug.h"
#include "Singleton.h"

/**
 * @brief This class contains the calibration data for the pulse normalization functions
 *
 * The calibration data is stored in /etc/autopilot/Calibration.xml.  This file is
 * read when the class is constructed (when the program starts) and is written to
 * each time a new calibration message is sent from QGroundControl.
 *
 * @author Bryan Godbolt <godbolt@ece.ualberta.ca>
 * @author Aakash Vasudevan <avasudev@ualberta.ca>
 * @author Nikos Vitzilaios <nvitzilaios@ualberta.ca>
 * @author Hasitha Senanayake <senanaya@ualberta.ca>
 * @date July 14, 2011: Created class
 * @date October 21, 2011 : Added functionality for storing calibration data in XML file
 */
class RadioCalibration : public Singleton<RadioCalibration>
{
    friend Singleton<RadioCalibration>;
public:

    /**
     * Writes all relevant values for this class to the unified System State object
     */
    void writeToSystemState();

    std::array<uint16_t, 3> getAileron();
    std::array<uint16_t, 3> getElevator();
    std::array<uint16_t, 5> getThrottle();
    std::array<uint16_t, 3> getRudder();
    std::array<uint16_t, 2> getGyro();
    std::array<uint16_t, 5> getPitch();
    /// Get position of flight mode switch
    std::array<uint16_t, 3> getFlightMode();

    /**
     * This function is called to change the calibration data.  After update the internal values,
     * the configuration file is updated with the new values
     * @param calibration_data vector of calibration info.  The contents must be aileron,
     * elevator, throttle, rudder, gyro, pitch.
     */
    void setCalibration(const std::vector<std::vector<uint16_t> >& calibration_data);

private:
    RadioCalibration();

    /**
     * Locks the calibration, and transfers n points from
     * setpoints to toset where n = numvals;
     *
     * @param setpoints - new setpoint values
     * @param toset - the vector to copy to
     */
    template<std::size_t N>
    void populateVector(const std::vector<uint16_t>& setpoints, std::array<uint16_t, N>& toset)
    {
        calibration_lock.lock();
        for(uint32_t i = 0; i < N; i++)
        {
            toset[i] = setpoints[i];
        }
        calibration_lock.unlock();
    };

    std::recursive_mutex calibration_lock;
    std::mutex calibration_file_lock;
    std::array<uint16_t, 2> gyro;

    std::array<uint16_t, 3> aileron;
    std::array<uint16_t, 3> elevator;
    std::array<uint16_t, 3> rudder;

    std::array<uint16_t, 5> throttle;
    std::array<uint16_t, 5> pitch;

    std::array<uint16_t, 3> flightMode;

    /// Read XML configuration file from heli::calibration_filename
    void loadFile();

    /**
     * Write the calibration info into an xml file.  The file is stored in
     * heli::calibration_filename.
     */
    void saveFile();

    /**
     * Converts a std::array or std::vector of setpoints to a comma separated list
     * @param setpoints array or vector of setpoints.  In fact any type can be used which
     * has size() and operator[](int) as members and whose contents have operator<<
     * defined.
     */
    template <typename T>
    std::string toString(const T& setpoints);

    /**
     * Parses a string delimited by commas in to a vector of uint16_t types.
     */
    void parseDelimiter(std::string input, std::vector<uint16_t>& output);

};

template <typename T>
std::string RadioCalibration::toString(const T& setpoints)
{
    std::stringstream ss;
    for (int i=0, size=0; i<(size=setpoints.size()); i++)
    {
        ss << setpoints[i];
        if (i < size-1)
            ss << ", ";
    }

    return ss.str();
}
#endif //RADIOCALIBRATION_H
