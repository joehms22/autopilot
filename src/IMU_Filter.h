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

#ifndef IMU_FILTER_H_
#define IMU_FILTER_H_

/* Boost Headers*/
#include <boost/circular_buffer.hpp>
#include <boost/array.hpp>

class IMU_Filter
{
public:
    IMU_Filter();
    virtual ~IMU_Filter();

    double operator()(double current_input);

    void reset();
private:
    std:array<double, 64> numerator_coeffs;
    boost::circular_buffer<double> inputs;
};

#endif
