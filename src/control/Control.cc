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

#include "Control.h"

#include <boost/algorithm/string/trim.hpp>


/* Project Headers */
#include "SystemState.h"
#include <servo_switch.h>
#include <string>
#include "bad_control.h"
#include "RCTrans.h"
#include "IMU.h"
#include "QGCLink.h"
#include "heli.h"
#include "Configuration.h"
#include "LogFile.h"

#include <functional>

// constants
std::string Control::XML_ROLL_MIX = "controller_params.mix.roll";
std::string Control::XML_PITCH_MIX = "controller_params.mix.pitch";
std::string Control::XML_CONTROLLER_MODE = "controller_params.mode";
std::string Control::XML_TRAJECTORY_VALUE = "controller_params.trajectory";
const std::string Control::LOG_POSITION_REFERENCE = "Position Reference Nav Frame";
const std::string Control::LOG_PID_TRANS_ATTITUDE_REF = "Translation PID Attitude Reference";
const std::string Control::LOG_SBF_TRANS_ATTITUDE_REF = "Translation SBF Attitude Reference";

Control::Control()
    :Logger("Control"),
    pilot_mix(6,1), // fill pilot_mix with 1s
     controller_mode(heli::Mode_Position_Hold_PID),
     mode_connection(QGCLink::getInstance()->control_mode.connect(
                         std::bind(&Control::set_controller_mode, this, std::placeholders::_1))),
     reference_position(3),
     trajectory_type(heli::Point_Trajectory)
{
    // load config file
    loadFile();
    reference_position.clear();


    // Set the huge map for lookups
    parameterSetMap[attitude_pid::PARAM_ROLL_KP] = [](double val){Control::getInstance()->attitude_pid_controller().set_roll_proportional(val);};
    parameterSetMap[attitude_pid::PARAM_ROLL_KD] = [](double val){Control::getInstance()->attitude_pid_controller().set_roll_derivative(val);};
    parameterSetMap[attitude_pid::PARAM_ROLL_KI] = [](double val){Control::getInstance()->attitude_pid_controller().set_roll_integral(val);};
    parameterSetMap[attitude_pid::PARAM_PITCH_KP] = [](double val){Control::getInstance()->attitude_pid_controller().set_pitch_proportional(val);};
    parameterSetMap[attitude_pid::PARAM_PITCH_KD] = [](double val){Control::getInstance()->attitude_pid_controller().set_pitch_derivative(val);};
    parameterSetMap[attitude_pid::PARAM_PITCH_KI] = [](double val){Control::getInstance()->attitude_pid_controller().set_pitch_integral(val);};
    parameterSetMap[PARAM_MIX_ROLL] = [](double val){Control::getInstance()->set_roll_mix(val);};
    parameterSetMap[PARAM_MIX_PITCH] = [](double val){Control::getInstance()->set_pitch_mix(val);};
    parameterSetMap[attitude_pid::PARAM_ROLL_TRIM] = [](double val){Control::getInstance()->attitude_pid_controller().set_roll_trim_degrees(val);};
    parameterSetMap[attitude_pid::PARAM_PITCH_TRIM] = [](double val){Control::getInstance()->attitude_pid_controller().set_pitch_trim_degrees(val);};
    parameterSetMap[translation_outer_pid::PARAM_X_KP] = [](double val){Control::getInstance()->translation_pid_controller().set_x_proportional(val);};
    parameterSetMap[translation_outer_pid::PARAM_X_KD] = [](double val){Control::getInstance()->translation_pid_controller().set_x_derivative(val);};
    parameterSetMap[translation_outer_pid::PARAM_X_KI] = [](double val){Control::getInstance()->translation_pid_controller().set_x_integral(val);};
    parameterSetMap[translation_outer_pid::PARAM_Y_KP] = [](double val){Control::getInstance()->translation_pid_controller().set_y_proportional(val);};
    parameterSetMap[translation_outer_pid::PARAM_Y_KD] = [](double val){Control::getInstance()->translation_pid_controller().set_y_derivative(val);};
    parameterSetMap[translation_outer_pid::PARAM_Y_KI] = [](double val){Control::getInstance()->translation_pid_controller().set_y_integral(val);};
    parameterSetMap[translation_outer_pid::PARAM_TRAVEL] = [](double val){Control::getInstance()->translation_pid_controller().set_scaled_travel_degrees(val);};
    parameterSetMap[tail_sbf::PARAM_TRAVEL] = [](double val){Control::getInstance()->x_y_sbf_controller.set_scaled_travel_degrees(val);};
    parameterSetMap[tail_sbf::PARAM_X_KP] = [](double val){Control::getInstance()->x_y_sbf_controller.set_x_proportional(val);};
    parameterSetMap[tail_sbf::PARAM_X_KD] = [](double val){Control::getInstance()->x_y_sbf_controller.set_x_derivative(val);};
    parameterSetMap[tail_sbf::PARAM_X_KI] = [](double val){Control::getInstance()->x_y_sbf_controller.set_x_integral(val);};
    parameterSetMap[tail_sbf::PARAM_Y_KP] = [](double val){Control::getInstance()->x_y_sbf_controller.set_y_proportional(val);};
    parameterSetMap[tail_sbf::PARAM_Y_KD] = [](double val){Control::getInstance()->x_y_sbf_controller.set_y_derivative(val);};
    parameterSetMap[tail_sbf::PARAM_Y_KI] = [](double val){Control::getInstance()->x_y_sbf_controller.set_y_integral(val);};
    parameterSetMap[circle::PARAM_HOVER_TIME] = [](double val){Control::getInstance()->circle_trajectory.set_hover_time(val);};
    parameterSetMap[circle::PARAM_RADIUS] = [](double val){Control::getInstance()->circle_trajectory.set_radius(val);};
    parameterSetMap[circle::PARAM_SPEED] = [](double val){Control::getInstance()->circle_trajectory.set_speed(val);};
    parameterSetMap[line::PARAM_HOVER_TIME] = [](double val){Control::getInstance()->line_trajectory.set_hover_time(val);};
    parameterSetMap[line::PARAM_SPEED] = [](double val){Control::getInstance()->line_trajectory.set_speed(val);};
    parameterSetMap[line::PARAM_X_TRAVEL] = [](double val){Control::getInstance()->line_trajectory.set_x_travel(val);};
    parameterSetMap[line::PARAM_Y_TRAVEL] = [](double val){Control::getInstance()->line_trajectory.set_y_travel(val);};
}


/* Set the parameter name strings */
const std::string Control::PARAM_MIX_ROLL = "MIX_ROLL";
const std::string Control::PARAM_MIX_PITCH = "MIX_PITCH";
const std::string Control::CONTROL_MODE = "MODE_CONTROL";

void Control::writeToSystemState()
{
}

std::vector<Parameter> Control::getParameters()
{
    // create vector to collect parameters from all controllers
    std::vector<Parameter> plist;

    // push pilot mix params
    plist.push_back(Parameter(PARAM_MIX_ROLL, pilot_mix[ROLL], heli::CONTROLLER_ID));
    plist.push_back(Parameter(PARAM_MIX_PITCH, pilot_mix[PITCH], heli::CONTROLLER_ID));
//	plist.push_back(Parameter(CONTROL_MODE, get_controller_mode(), heli::CONTROLLER_ID));

    // append parameters from pid controller
    std::vector<Parameter> controller_params(attitude_pid_controller().getParameters());
    plist.insert(plist.begin() + plist.size(), controller_params.begin(), controller_params.end());

    std::vector<Parameter> translation_controller_params(translation_pid_controller().getParameters());
    plist.insert(plist.end(), translation_controller_params.begin(), translation_controller_params.end());

    std::vector<Parameter> sbf_controller_params(x_y_sbf_controller.getParameters());
    plist.insert(plist.end(), sbf_controller_params.begin(), sbf_controller_params.end());

    std::vector<Parameter> line_params(line_trajectory.getParameters());
    plist.insert(plist.end(), line_params.begin(), line_params.end());

    std::vector<Parameter> circle_params(circle_trajectory.getParameters());
    plist.insert(plist.end(), circle_params.begin(), circle_params.end());

    // append parameters from any other controllers here

    // return the complete parameter list
    return plist;
}

bool Control::setParameter(Parameter p)
{
    std::string param_id(p.getParamID());
    boost::trim(param_id);

    // Make sure the param id is in the map.
    if(parameterSetMap.find(param_id) == parameterSetMap.end())
    {
        warning() << "Control::setParameter - unknown parameter: " << p;
        return false;
    }

    parameterSetMap[param_id](p.getValue());
    saveFile();
    writeToSystemState();
    return true;
}

blas::vector<double> Control::get_control_effort() const
{
    std::vector<double> pilot_inputs(RCTrans::getScaledVector());

    // compute control effort
    blas::vector<double> control_effort(attitude_pid_controller().get_control_effort());
    control_effort.resize(6);

    if (!(pilot_inputs.size() == control_effort.size() && pilot_inputs.size() == 6 && pilot_inputs.size() == pilot_mix.size()))
    {
        bad_control b("At least one of the vectors are not of length 6", std::string(__FILE__), __LINE__);
        throw b;
    }

    blas::vector<double> control_output(6);

    // mix according to pilot_mix
    for (unsigned int i=0; i < control_output.size(); i++)
    {
        if (!(pilot_mix[i] >= 0 && pilot_mix[i] <= 1))
        {
            bad_control b(" Pilot mix values is out of range.", std::string(__FILE__), __LINE__);
            throw b;
        }
        control_output[i] = pilot_mix[i]*pilot_inputs[i] + (1-pilot_mix[i])*control_effort[i];
    }

    // log control effort
    LogFile::getInstance()->logData("Control Effort", control_effort);
    //log final control output
    LogFile::getInstance()->logData("Mixed Control Output", control_output);
    return control_output;
}

void Control::set_roll_mix(double roll_mix)
{
    if (roll_mix <= 1 && roll_mix >= 0)
    {
        {
            std::lock_guard<std::mutex> lock(pilot_mix_lock);
            pilot_mix[ROLL] = roll_mix;
        }
        message() << "Changed roll pilot mix to: " << roll_mix;
    }
    else
        message() << "Invalid roll mix argument: " << roll_mix;
}

void Control::set_pitch_mix(double pitch_mix)
{

    if (pitch_mix <= 1 && pitch_mix >= 0)
    {
        {
            std::lock_guard<std::mutex> lock(pilot_mix_lock);
            pilot_mix[PITCH] = pitch_mix;
        }
        message() << "Changed pitch pilot mix to: " << pitch_mix;
    }

    else
        message() << "Invalid pitch mix argument: " << pitch_mix;
}

double Control::get_roll_mix() const
{
    std::lock_guard<std::mutex> lock(pilot_mix_lock);
    return pilot_mix[ROLL];
}

double Control::get_pitch_mix() const
{
    std::lock_guard<std::mutex> lock(pilot_mix_lock);
    return pilot_mix[PITCH];
}



void Control::loadFile()
{
    // set the configuration for this control sequence
    Configuration* cfg = Configuration::getInstance();

    set_roll_mix(cfg->getd(XML_ROLL_MIX, get_roll_mix()));
    set_pitch_mix(cfg->getd(XML_PITCH_MIX, get_pitch_mix()));
    set_controller_mode(static_cast<heli::Controller_Mode>(cfg->geti(XML_CONTROLLER_MODE, get_controller_mode())));
    set_trajectory_type(static_cast<heli::Trajectory_Type>(cfg->geti(XML_TRAJECTORY_VALUE, get_trajectory_type())));

    // set up the configuration for all of the other controls

    attitude_pid_controller().parse_pid();
    translation_pid_controller().parse_xml_node();
    x_y_sbf_controller.parse_xml_node();

    line_trajectory.parse_xml_node();
    circle_trajectory.parse_xml_node();
}

void Control::operator()()
{
    blas::vector<double> reference_position(get_reference_position());
    LogFile::getInstance()->logData(LOG_POSITION_REFERENCE, reference_position);

    if (get_controller_mode() == heli::Mode_Position_Hold_PID)
    {
        if (translation_pid_controller().runnable())
        {
            try
            {
                translation_pid_controller()(reference_position);
                blas::vector<double> roll_pitch_reference(translation_pid_controller().get_control_effort());
                set_reference_attitude(roll_pitch_reference);
                LogFile::getInstance()->logData(LOG_PID_TRANS_ATTITUDE_REF, roll_pitch_reference);
                attitude_pid_controller()(roll_pitch_reference);
            }
            catch (bad_control& bc)
            {
                warning() << "Caught exception from Translational PID, switching to attitude stabilization mode";
                set_controller_mode(heli::Mode_Attitude_Stabilization_PID);
            }
        }
        else
        {
            warning() <<"Control: translation pid controller reports it is not runnable.  Switching to attitude control.";
            set_controller_mode(heli::Mode_Attitude_Stabilization_PID);
        }

        // prevents exception being thrown
        return;
    }
    else if (get_controller_mode() == heli::Mode_Position_Hold_SBF)
    {
        if (x_y_sbf_controller.runnable())
        {
            try
            {
                x_y_sbf_controller(reference_position);
                blas::vector<double> attitude_reference(x_y_sbf_controller.get_control_effort());
                set_reference_attitude(attitude_reference);
                LogFile::getInstance()->logData(LOG_SBF_TRANS_ATTITUDE_REF, attitude_reference);
                attitude_pid_controller()(attitude_reference);
            }
            catch (bad_control& bc)
            {
                warning() << "Caught exception from Translation SBF Controller, switching to attitude stabilization.";
                set_controller_mode(heli::Mode_Attitude_Stabilization_PID);
            }
        }
        else
        {
            warning() <<"Control: translation sbf controller reports it is not runnable.  Switching to attitude control.";
            set_controller_mode(heli::Mode_Attitude_Stabilization_PID);
        }
        return;
    }
    // not else if so that it will run if the mode was changed
    if (get_controller_mode() == heli::Mode_Attitude_Stabilization_PID)
    {
        blas::vector<double> roll_pitch_reference(2);
        roll_pitch_reference[ROLL] = attitude_pid_controller().get_roll_trim_radians();
        roll_pitch_reference[PITCH] = attitude_pid_controller().get_pitch_trim_radians();
        set_reference_attitude(roll_pitch_reference);
        attitude_pid_controller()(roll_pitch_reference);

        // prevents exception being thrown
        return;
    }
    throw bad_control("Control: not set to valid control mode");
}

void Control::saveFile()
{
    /* get pid params */
    attitude_pid_controller().get_xml_node();

    /* get trans pid params */
    translation_pid_controller().get_xml_node();

    /* get sbf params */
    x_y_sbf_controller.get_xml_node();

    /* get circle params */
    circle_trajectory.get_xml_node();

    /* get line params */
    line_trajectory.get_xml_node();

    /* add pilot mixes */

    Configuration* cfg = Configuration::getInstance();

    cfg->setd(XML_ROLL_MIX, pilot_mix[ROLL]);
    cfg->setd(XML_PITCH_MIX, pilot_mix[PITCH]);
    cfg->seti(XML_CONTROLLER_MODE, (int) get_controller_mode());
    cfg->seti(XML_TRAJECTORY_VALUE, (int) get_trajectory_type());
}

std::string Control::getModeString(heli::Controller_Mode mode)
{
    if (mode == heli::Mode_Attitude_Stabilization_PID)
        return "ATTITUDE_PID";
    else if (mode == heli::Mode_Position_Hold_PID)
        return "POSITION_PID";
    else if (mode == heli::Mode_Position_Hold_SBF)
        return "POSITION_SBF";
    return std::string();
}

std::string Control::getTrajectoryString(heli::Trajectory_Type trajectory_type)
{
    switch(trajectory_type)
    {
    case heli::Point_Trajectory:
        return "Point_Trajectory";
    case heli::Line_Trajectory:
        return "Line_Trajectory";
    case heli::Circle_Trajectory:
        return "Circle_Trajectory";
    default:
        return "Unknown Trajectory type";
    }
}

void Control::set_reference_position()
{
    blas::vector<double> reference(IMU::getInstance()->get_ned_position());
    set_reference_position(reference);
    reset();
    message() << "Control: Position reference set to: " << reference;
}
void Control::set_controller_mode(heli::Controller_Mode mode)
{
    bool mode_changed = false;
    if (mode < heli::Num_Controller_Modes)
    {
        std::lock_guard<std::mutex> lock(controller_mode_lock);
        if (controller_mode != mode)
            mode_changed = true;
        controller_mode = mode;
    }
    if (mode_changed)
    {
        this->mode_changed(mode);
        warning() << "Controller mode changed to: " << getModeString(mode);
        saveFile();
        writeToSystemState();
    }
}

void Control::reset()
{
    x_y_pid_controller.reset();
    roll_pitch_pid_controller.reset();
    x_y_sbf_controller.reset();
    line_trajectory.reset();
    circle_trajectory.reset();
}

bool Control::runnable() const
{
    // control is runnable as long as attitude can still be controlled
    return attitude_pid_controller().runnable();
}

blas::vector<double> Control::get_reference_position() const
{
    if (get_trajectory_type() == heli::Line_Trajectory)
    {
        return line_trajectory.get_reference_position();
    }
    else if (get_trajectory_type() == heli::Circle_Trajectory)
    {
        return circle_trajectory.get_reference_position();
    }
    else //(get_trajectory_type() == heli::Point_Trajectory)
    {
        std::lock_guard<std::mutex> lock(reference_position_lock);
        return reference_position;
    }
}

void Control::set_trajectory_type(const heli::Trajectory_Type trajectory_type)
{
    bool type_changed = false;
    if (trajectory_type < heli::Num_Trajectories)
    {
        std::lock_guard<std::mutex> lock(trajectory_type_lock);
        if (this->trajectory_type != trajectory_type)
        {
            type_changed = true;
            this->trajectory_type = trajectory_type;
        }
    }
    if (type_changed)
    {
        warning() << "Trajectory type changed to: " << getTrajectoryString(trajectory_type);
        saveFile();
        writeToSystemState();
    }
}
