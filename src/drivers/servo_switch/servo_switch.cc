/**************************************************************************
 * Copyright 2012 Bryan Godbolt
 * Copyright 2013 Joseph Lewis <joehms22@gmail.com>
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
 *************************************************************************/


/* c headers */
#include <stdint.h>
#include <bitset>
#include <fcntl.h>

// stl headers
#include "Debug.h"
#include "LogFile.h"
#include "heli.h"
#include "SystemState.h"

/* File Handling Headers */
#include "servo_switch.h"
#include "RateLimiter.h"

// As defined in section 4.2 of the February 2, 2007 SSC Manual
enum ServoMessageID
{
    // SSC TO HOST
    STATUS=10,
    CHANNEL_SOURCE=11,
    PULSE_OUTPUTS=12,
    PULSE_INPUTS=13,
    AUXILIARY_INPUTS=14,
    SYSTEM_CONFIGURATION=15,

    // HOST TO SSC
    PULSE_COMMAND=20,
    AUXILIARY_OUTPUTS=21,
    LOCKOUT_NOW=98
};

// path to serial device connected to gx3
const std::string SERVO_SERIAL_PORT_CONFIG_NAME = "servo.serial_port";
const std::string SERVO_SERIAL_PORT_CONFIG_DEFAULT = "/dev/ttyS0";

const std::string servo_switch::LOG_INPUT_PULSE_WIDTHS = "Input Pulse Widths";
const std::string servo_switch::LOG_OUTPUT_PULSE_WIDTHS = "Output Pulse Widths";
const std::string servo_switch::LOG_INPUT_RPM = "Engine RPM";


servo_switch::servo_switch()
    : Driver("Servo Switch","servo"),
      raw_inputs(9, 0),
      raw_outputs(9, 0),
      pilot_mode(heli::PILOT_UNKNOWN)
{
    if(!isEnabled())
    {
        warning() << "Servo switch disabled!";
        return;
    }

    if(init_port())
    {
        receive = std::thread(read_serial());
        send = std::thread(send_serial());
        LogFile *log = LogFile::getInstance();
        log->logHeader(LOG_INPUT_PULSE_WIDTHS, "CH1 CH2 CH3 CH4 CH5 CH6 CH7 CH8 CH9");
        log->logHeader(LOG_OUTPUT_PULSE_WIDTHS, "CH1 CH2 CH3 CH4 CH5 CH6 CH7 CH8 CH9");
        log->logHeader(LOG_INPUT_RPM, "RPM");
    }
    else
    {
        initFailed("Could not init serial port");
    }
}

void servo_switch::writeToSystemState()
{
    SystemState *state = SystemState::getInstance();

    auto vec = get_raw_inputs();
    std::array<uint16_t, 8> raw;
    std::copy_n(vec.begin(), 8, raw.begin());
    state->servoRawInputs.set(raw, 0);


    state->state_lock.lock();
    state->servo_raw_outputs = raw_outputs;
    state->servo_pilot_mode.store(pilot_mode.load());
    state->state_lock.unlock();
}

bool servo_switch::init_port()
{
    std::string port = configGets("serial_port", SERVO_SERIAL_PORT_CONFIG_DEFAULT);

    debug() << "Servo switch: port is " << port;

    fd_ser1 = open(port.c_str(), O_RDWR | O_NOCTTY);// | O_NDELAY);

    if(fd_ser1 == -1)
    {
        critical() << "Unable to open port " << port;
        return false;
    }

    debug() << "Servo switch: opened";

    namedTerminalSettings("switch1", fd_ser1, 115200, "8N1", false, true);

    return true;
}

void servo_switch::set_pilot_mode(heli::PILOT_MODE mode)
{
    if (pilot_mode != mode)
    {
        pilot_mode = mode;
        message() << "Pilot mode changed to: " << heli::PILOT_MODE_DESCRIPTOR[mode];
        pilot_mode_changed(mode);
    }
}

std::vector<uint8_t> servo_switch::compute_checksum(uint8_t id, uint8_t count, const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> checksum(2, 0);
    checksum[0] = id + count;
    checksum[1] = 2*id + count;

    for (std::vector<uint8_t>::const_iterator it = payload.begin(); it != payload.end(); ++it)
    {
        checksum[0] += *it;
        checksum[1] += checksum[0];
    }
    return checksum;
}

/* read_serial functions */
void servo_switch::read_serial::read_data()
{
    servo_switch* servo = servo_switch::getInstance();

    int fd_ser = servo->fd_ser1;
    std::vector<uint8_t> payload;
    std::vector<uint8_t> checksum(2);
    while(! servo->terminateRequested())
    {
        servo->trace() << "searching for header";
        find_next_header();

        // get message id
        uint8_t id = 0;
        int idb = servo->readDevice(fd_ser, &id, 1);

        // get message count
        uint8_t count = 0;
        int ctb = servo->readDevice(fd_ser, &count, 1);

        // allocate space for message
        payload.resize(count);
        // get message payload
        int pldb = servo->readDevice(fd_ser, &payload[0], count);

        // zero checksum
        checksum.assign(checksum.size(), 0);
        // get checksum
        servo->readDevice(fd_ser, &checksum[0], 2);

        servo->trace() << "ID ("<< idb <<"): " << id << " Count("<< ctb <<"): " << count << " payload("<< pldb<< "): " << count;

        if (checksum == compute_checksum(id, count, payload))
        {
            servo->trace() << "parsing message";
            parse_message(id, payload);
        }
        else
        {
            servo->trace() << "bad checksum";
        }
    }
}

void servo_switch::read_serial::parse_message(uint8_t id, const std::vector<uint8_t>& payload)
{
    servo_switch* servo = getInstance();

    switch (id)
    {
    case STATUS:
    {
        uint16_t status =  payload[1];
        // shift right to get command channel state
        status = (status & 0x6) >> 1;
        /** Section 4.2.1.1 of Servo Switch/Controller Users Manual February 2, 2007
        0 = signal not present
        1 = 1ms
        2 = 1.5ms
        3 = 2ms
        **/
        switch (status)
        {
        case 1: // 1ms
            servo->set_pilot_mode(heli::PILOT_MANUAL);
            break;
        case 2: // 1.5ms
        case 3: // 2ms
            servo->set_pilot_mode(heli::PILOT_AUTO);
            break;
        default:
            servo->warning("Command channel state signal not present on servo switch");
        }
        break;
    }

    case PULSE_INPUTS:
        parse_pulse_inputs(payload);
        break;

    case AUXILIARY_INPUTS:
        parse_aux_inputs(payload);
        break;

    default:
        servo->debug() << "Received unknown message from servo switch id: " << id;
    }
}

void servo_switch::read_serial::parse_pulse_inputs(const std::vector<uint8_t>& payload)
{
    servo_switch* servo = getInstance();
    servo->debug("Parsing pulse inputs");
    const uint16_t upper_limit = 2200;
    const uint16_t lower_limit = 800;
    std::vector<uint16_t> pulse_inputs(getInstance()->get_raw_inputs());  // no need for more than 9 channels
    uint16_t pulse_width = 0;
    for (uint32_t i=1; i<payload.size()/2 && i < pulse_inputs.size(); i++)
    {
        pulse_width = (static_cast<uint16_t>(payload[i*2]) << 8) + payload[i*2+1];
        servo->debug("Got pulse input: ");
        servo->debug() << "Input " << i << "Payload: " << pulse_width;
        if (pulse_width > lower_limit && pulse_width < upper_limit)
        {
            pulse_inputs[i-1] = pulse_width;
        }
    }
    // treat ch8 differently
    pulse_inputs[7] = (static_cast<uint16_t>(payload[0]) << 8) + payload[1];

    getInstance()->set_raw_inputs(pulse_inputs);
    LogFile *log = LogFile::getInstance();
    log->logData(LOG_INPUT_PULSE_WIDTHS, pulse_inputs);
    getInstance()->writeToSystemState();

}

void servo_switch::read_serial::parse_aux_inputs(const std::vector<uint8_t>& payload)
{
    servo_switch& ss = *servo_switch::getInstance();


    std::bitset<8> meas_byte (payload[2]);
    if(meas_byte.test(7))
    {
        ss.debug("Time measurement over range");
        return ;
    }

    meas_byte.set(7,0);
    meas_byte.set(6,0);

    uint16_t time_measurement;
    time_measurement = (static_cast<uint16_t>(meas_byte.to_ulong()) << 8) + payload[3];

    // TODO extract out these constants to meaningful variables - Joseph
    double speed = 1 / (time_measurement*32.0*0.000001);
    std::vector<double> speeds;
    speeds.push_back(speed);
    speeds.push_back(speed);

    LogFile *log = LogFile::getInstance();
    log->logData(LOG_INPUT_RPM, speeds);
    getInstance()->writeToSystemState();
}


void servo_switch::read_serial::find_next_header()
{
    servo_switch* servo = servo_switch::getInstance();
    servo->trace() << "Finding next header";
    bool synchronized = false;
    int fd_ser = servo->fd_ser1;

    bool found_first_byte = false;
    uint8_t buf;
    while (!synchronized && !servo->terminateRequested())
    {
        servo->readDevice(fd_ser, &buf, 1);
        if (buf == 0x81) // first byte of header
            found_first_byte = true;
        else if (buf == 0xA1 && found_first_byte)
            synchronized = true;
        else
            found_first_byte = false;
    }
}
/* send_serial functions */

void servo_switch::send_serial::operator()()
{
    servo_switch* servo = getInstance();

    RateLimiter rl(50);
    while(true)
    {
        rl.wait();

        // Construct outgoing message.
        std::vector<uint16_t> raw_outputs(servo->get_raw_outputs());
        std::vector<uint8_t> pulse_message {0x81, 0xA1, 20};

        pulse_message.push_back(raw_outputs.size() * 2);
        for (uint32_t i = 0; i < raw_outputs.size(); i++)
        {
            pulse_message.push_back(static_cast<uint8_t>(raw_outputs[i] >> 8));
            pulse_message.push_back(static_cast<uint8_t>(raw_outputs[i] & 0xFF));
        }

        std::vector<uint8_t> checksum = compute_checksum(20, raw_outputs.size() * 2,
                                                         std::vector<uint8_t>(pulse_message.begin()+4,
                                                                              pulse_message.end()));

        pulse_message.push_back(checksum[0]);
        pulse_message.push_back(checksum[1]);

        // Log our data.
        LogFile::getInstance()->logData(LOG_OUTPUT_PULSE_WIDTHS, raw_outputs);

        // Send message to servo switch.
        while (write(servo->fd_ser1, &pulse_message[0], pulse_message.size()) < 0)
        {
            servo->debug("Error sending pulse output message to servo switch");
        }

        rl.finishedCriticalSection();
    }
}
