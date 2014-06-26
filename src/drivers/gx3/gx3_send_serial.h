/*******************************************************************************
 * Copyright 2012 Bryan Godbolt
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

#ifndef GX3_SEND_SERIAL_H_
#define GX3_SEND_SERIAL_H_

#include "IMU.h"
#include "Debug.h"

#include <mutex>

/* Boost Headers */
#include <boost/signals2/signal.hpp>
#include <boost/function.hpp>


/**
 * Class to send commands to 3DM-GX3
 * @author Bryan Godbolt <godbolt@ece.ualberta.ca>
 * @date February 3, 2012: Class creation
 * @date May 2, 2012: Added novatel external measurement
 */
class IMU::send_serial
{
public:
    send_serial(IMU* parent = 0);
//	void operator()();
private:
    /// send the sequence of messages necessary to initialize the imu
    void init_imu();
    /// ping the imu
    void ping();
    /// tell the imu to stop sending messages
    void set_to_idle();
    /// choose which ahrs data to receive
    void ahrs_message_format();
    /// choose which nav data to receive
    void nav_message_format();
    /// enable continuous sending of nav and ahrs messages
    void enable_messages();
    /// set the parameters of the navigation filter
    void set_filter_parameters();
    /// reset the navigation filter
    void reset_filter();
    /// set the initial filter attitude from the AHRS
    void init_filter();
    /// update the gx3 with the novatel measurement @note llh converted to degrees for transmission to gx3
    void external_gps_update();

    /**
     * Finishes a packet by computing and appending the checksum, sending it, creating
     * an ack from the given command, and waiting for it to return;
     *
     * returns the ack error code
     */
    uint8_t finish_packet(std::vector<uint8_t> &vec, uint8_t ack_command);

    /**
     * Finishes a packet by computnig and appending the checksum, sending it, creating
     * an ack from the command and waiting for it to return.
     *
     * If ack is not an error, prints a message. Else, prints a warning and the error
     * code using the human_command_name param to describe where the error came from.
     *
     * returns the ack error code.
     */
    uint8_t finish_packet_and_alert(
        std::vector<uint8_t> &vec,
        uint8_t ack_command,
        std::string human_command_name);

    /**
     * Waits for the ack to finish, on success sends a message, on failure
     * sends a warning and sets the GX3 status message.
     */
    void on_error_set_status(ack_handler& ack, std::string human_ack_name);


    template <typename floating_type>
    static std::vector<uint8_t> float_to_raw(const floating_type f);

    template <typename IntegerType>
    static std::vector<uint8_t> int_to_raw(const IntegerType i);

    /// append the raw version of f onto message
    template <typename floating_type>
    static void pack_float(const floating_type f, std::vector<uint8_t>& message);

    template<typename IntegerType>
    static void pack_int(const IntegerType i, std::vector<uint8_t>& message);

    /// serialize messages being sent to imu.  any thread intending to write data to the serial port should lock this mutex
    mutable std::mutex send_lock;

    /// start a thread to send a message
    template <typename Callable>
    void start_send_thread(Callable f);

    /// connection between QGCLink::reset_filter and a thread to run reset_filter()
    boost::signals2::scoped_connection reset_connection;
    /// connection for QGCLink::init_filter
    boost::signals2::scoped_connection init_filter_connection;
    /// connection to update gps measurement
    boost::signals2::scoped_connection gps_update_connection;
    /// connection to re-initialize the imu when the autopilot stops receiving data
    boost::signals2::scoped_connection initialize_imu_connection;
};

template<typename Callable>
void IMU::send_serial::start_send_thread(Callable f)
{
    new std::thread(f);
}

template <typename floating_type>
std::vector<uint8_t> IMU::send_serial::float_to_raw(const floating_type f)
{
    std::vector<uint8_t> result;
    const uint8_t* byte = reinterpret_cast<const uint8_t*>(&f);
    for (int i = sizeof(floating_type) - 1; i >= 0; i--)
        result.push_back(byte[i]);
    return result;
}

template <typename IntegerType>
std::vector<uint8_t> IMU::send_serial::int_to_raw(const IntegerType i)
{
    std::vector<uint8_t> result;
    const uint8_t* byte = reinterpret_cast<const uint8_t*>(&i);
    for (int j= sizeof(IntegerType) - 1; j>= 0; j--)
    {
        result.push_back(byte[j]);
    }
    return result;
}

template<typename floating_type>
void IMU::send_serial::pack_float(const floating_type f, std::vector<uint8_t>& message)
{
    std::vector<uint8_t> raw(float_to_raw(f));
    message.insert(message.end(), raw.begin(), raw.end());
}

template<typename IntegerType>
void IMU::send_serial::pack_int(const IntegerType i, std::vector<uint8_t>& message)
{
    std::vector<uint8_t> raw(int_to_raw(i));
    message.insert(message.end(), raw.begin(), raw.end());
}

#endif
