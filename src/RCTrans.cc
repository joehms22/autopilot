/**************************************************************************
 * Copyright 2012 Bryan Godbolt, Hasitha Senanayake
 * Copyright 2014 Joseph Lewis <joseph@josephlewis.net>
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

#include "RCTrans.h"

RCTrans::RCTrans()
{

}

double RCTrans::pulse2norm(uint16_t pulse, std::array<uint16_t, 2> setpoint)
{
    double pulseMicros = pulse;
    double normalizedPulse = 0;
    if(setpoint[1] > setpoint[0])  // normal switch mapping
    {
        /* About the first set point, with tolerance of 10us for channel noise variation, saturate for lower values */
        if(pulseMicros <= (setpoint[0] + 10))
            normalizedPulse = AVCS_HEADING_HOLD_MODE;
        /* About the second set point, with tolerance of 10us, saturate higher values */
        else if(pulseMicros >= (setpoint[1] - 10))
            normalizedPulse = NORMAL_GYRO_MODE;
        /* Between the first & second set points */
        else
            normalizedPulse = AVCS_HEADING_HOLD_MODE;
    }
    else if(setpoint[1] < setpoint[0])  //reverse switch mapping
    {
        /* About the first set point, with tolerance of 10us for channel noise variation, saturate for higher values */
        if(pulseMicros >= (setpoint[0] - 10))
            normalizedPulse = AVCS_HEADING_HOLD_MODE;
        /* About the second set point, with tolerance of 10us, saturate for lower values */
        else if(pulseMicros <= (setpoint[1] + 10))
            normalizedPulse = NORMAL_GYRO_MODE;
        /* Between the first & second set points */
        else
            normalizedPulse = AVCS_HEADING_HOLD_MODE;
    }
    return normalizedPulse;
}

double RCTrans::pulse2norm(uint16_t pulse, std::array<uint16_t, 3> setpoint)
{
    double pulseMicros = pulse;
    double normalizedPulse = 0;

    if(setpoint[2]>setpoint[1])   // normal stick mapping
    {
        if((pulseMicros >= setpoint[0]) && (pulseMicros <= setpoint[2]))   // within bounds
        {
            if(pulseMicros >= setpoint[1])
                normalizedPulse = (pulseMicros - setpoint[1])/(setpoint[2] - setpoint[1]);
            else
                normalizedPulse = (pulseMicros - setpoint[1])/(setpoint[1] - setpoint[0]);
        }
        else if(pulseMicros < setpoint[0])  // saturate if outside bounds
            normalizedPulse = -1.0;
        else if(pulseMicros > setpoint[2])  // saturate if outside bounds
            normalizedPulse = 1.0;
    }
    else        // reverse stick mapping
    {
        if((pulseMicros >= setpoint[2]) && (pulseMicros <= setpoint[0]))  // within bounds
        {
            if(pulseMicros <= setpoint[1])
                normalizedPulse = (setpoint[1] - pulseMicros)/(setpoint[1] - setpoint[2]);
            else
                normalizedPulse = (setpoint[1] - pulseMicros)/(setpoint[0] - setpoint[1]);
        }
        else if(pulseMicros < setpoint[2])        // saturate if outside bounds
            normalizedPulse = 1.0;
        else if(pulseMicros > setpoint[0])        // saturate if outside bounds
            normalizedPulse = -1.0;
    }
    return normalizedPulse;
}

double RCTrans::pulse2norm(uint16_t pulse, std::array<uint16_t, 5> setpoint)
{
    double pulseMicros = pulse;
    double normalizedPulse = 0;
    if(setpoint[4] > setpoint[0])         // Normal stick mapping.
    {
        if((pulseMicros >= setpoint[0]) && (pulseMicros <= setpoint[4]))
        {
            if(pulseMicros <= setpoint[1])
                normalizedPulse = ((pulseMicros - setpoint[0])/(setpoint[1] - setpoint[0])) * 0.25;
            else if((pulseMicros > setpoint[1]) && (pulseMicros <= setpoint[2]))
                normalizedPulse = (((pulseMicros - setpoint[1])/(setpoint[2] - setpoint[1])) * 0.25) + 0.25;
            else if((pulseMicros > setpoint[2]) && (pulseMicros <= setpoint[3]))
                normalizedPulse =  (((pulseMicros - setpoint[2])/(setpoint[3] - setpoint[2])) * 0.25) + 0.50;
            else if((pulseMicros > setpoint[3]) && (pulseMicros <= setpoint[4]))
                normalizedPulse =  (((pulseMicros - setpoint[3])/(setpoint[4] - setpoint[3])) * 0.25) + 0.75;
        }
        else if(pulseMicros < setpoint[0])    // saturate for out of bounds
            normalizedPulse = 0.0;
        else if(pulseMicros > setpoint[4])    // saturate for out of bounds
            normalizedPulse = 1.0;
    }
    else if(setpoint[0] > setpoint[4])    // Reverse stick mapping.
    {
        if((pulseMicros <= setpoint[0]) && (pulseMicros >= setpoint[4]))
        {
            if(pulseMicros >= setpoint[1])
                normalizedPulse = ((pulseMicros - setpoint[0])/(setpoint[1] - setpoint[0])) * 0.25;
            else if((pulseMicros < setpoint[1]) && (pulseMicros >= setpoint[2]))
                normalizedPulse = (((pulseMicros - setpoint[1])/(setpoint[2] - setpoint[1])) * 0.25) + 0.25;
            else if((pulseMicros < setpoint[2]) && (pulseMicros >= setpoint[3]))
                normalizedPulse =  (((pulseMicros - setpoint[2])/(setpoint[3] - setpoint[2])) * 0.25) + 0.50;
            else if((pulseMicros < setpoint[3]) && (pulseMicros >= setpoint[4]))
                normalizedPulse =  (((pulseMicros - setpoint[3])/(setpoint[4] - setpoint[3])) * 0.25) + 0.75;
        }
        else if(pulseMicros > setpoint[0])    // saturate for out of bounds
            normalizedPulse = 0.0;
        else if(pulseMicros < setpoint[4])    // saturate for out of bounds
            normalizedPulse = 1.0;
    }

    return normalizedPulse;
}


std::vector<double> RCTrans::getScaledVector()
{
    std::vector<double> norms(6);

    auto ss = servo_switch::getInstance();
    auto rc = RadioCalibration::getInstance();

    norms[AILERON] =    pulse2norm(ss->getRaw(heli::CH1), rc->getAileron());
    norms[ELEVATOR] =   pulse2norm(ss->getRaw(heli::CH2), rc->getElevator());
    norms[THROTTLE] =   pulse2norm(ss->getRaw(heli::CH3), rc->getThrottle());
    norms[RUDDER] =     pulse2norm(ss->getRaw(heli::CH4), rc->getRudder());
    norms[GYRO] =       pulse2norm(ss->getRaw(heli::CH5), rc->getGyro());
    norms[PITCH] =      pulse2norm(ss->getRaw(heli::CH6), rc->getPitch());

    return norms;
}
