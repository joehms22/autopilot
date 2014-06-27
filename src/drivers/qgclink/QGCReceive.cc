/**************************************************************************
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
 *************************************************************************/

#include "QGCReceive.h"

/* Project Headers */
#include "Control.h"
#include "IMU.h"
#include "RadioCalibration.h"
#include "Helicopter.h"
#include "Driver.h"
#include "CommonMessages.h"

/* Mavlink Headers */
#include "mavlink.h"

/* Boost Headers */
#include <boost/array.hpp>
#include <boost/asio.hpp>
using boost::asio::ip::udp;
using boost::asio::ip::address;


void QGCLink::QGCReceive::receive()
{
	mavlink_message_t msg;
	mavlink_status_t status;

	if (qgc == NULL)
		qgc = QGCLink::getInstance();

	std::array<char, 128> recv_buf;
	for (;;)
	{
		// pull a datagram from the socket
		int bytes_received = 0;
		try
		{
			bytes_received = qgc->socket.receive_from(boost::asio::buffer(recv_buf), qgc->qgc);
		}
		catch (boost::system::system_error e)
		{
			std::cerr << e.what() << std::endl;
		}
		for (int i=0; i<bytes_received; i++)
		{
			if(mavlink_parse_char(MAVLINK_COMM_0, recv_buf[i], &msg, &status))
			{

                for(Driver* d : Driver::getDrivers())
                {
                    d->recvMavlinkMsg(msg);
                }


				switch(msg.msgid)
				{
//				case MAVLINK_MSG_ID_SET_MODE:
//				{
//					mavlink_set_mode_t mode;
//					mavlink_msg_set_mode_decode(&msg, &mode);
//					switch (mode.mode)
//					{
//					case MAV_MODE_MANUAL:
//						MainApp::request_mode(heli::MODE_DIRECT_MANUAL);
//						warning() << "Switch to Direct Manual Mode";
//						break;
//					case MAV_MODE_TEST1:
//						MainApp::request_mode(heli::MODE_SCALED_MANUAL);
//						warning() << "Switch to Scaled Manual Mode";
//						break;
//					case MAV_MODE_AUTO:
//						MainApp::request_mode(heli::MODE_AUTOMATIC_CONTROL);
//						warning() << "Switch to Automatic Control Mode";
//						break;
//					default:
//						critical() << "Unknown Mode";
//					}
//
//					break;
//				}
//				case MAVLINK_MSG_ID_ACTION:
//					debug() << "Execute an action.";
//					mavlink_action_t action;
//					mavlink_msg_action_decode(&msg, &action);
//					switch(action.action)
//					{
//					case MAV_ACTION_CALIBRATE_RC:
//					{
//						debug() << "Send RC Calibration Data";
//						qgc->requested_rc_calibration_lock.lock();
//						qgc->requested_rc_calibration = true;
//						qgc->requested_rc_calibration_lock.unlock();
//						break;
//					}
//					case MAV_ACTION_SHUTDOWN:
//					{
//						warning() << "Received halt from QGC.  Sending terminate signal";
//						MainApp::terminate();
//						break;
//					}
//					default:
//						warning() << " Unknown Action: " << action.action;
//						break;
//					}
//					break;
				case MAVLINK_MSG_ID_UALBERTA_ACTION:
				{
					qgc->debug() << "Received Ualberta Action";
					mavlink_ualberta_action_t action;
					mavlink_msg_ualberta_action_decode(&msg, &action);
					switch (action.action)
					{
					case UALBERTA_RC_CALIBRATION:
					{
						qgc->debug() << "Send RC Calibration Data";
						qgc->set_requested_rc_calibration();
						break;
					}
					case UALBERTA_SET_ORIGIN:
					{
						qgc->debug() << "Set Origin";
						IMU::getInstance()->set_ned_origin();
						break;
					}
					case UALBERTA_SET_SERVO_SOURCE:
					{
						switch(action.param)
						{
						case UALBERTA_MODE_MANUAL_DIRECT:
							qgc->servo_source(heli::MODE_DIRECT_MANUAL);
							break;
						case UALBERTA_MODE_MANUAL_SCALED:
							qgc->servo_source(heli::MODE_SCALED_MANUAL);
							break;
						case UALBERTA_MODE_AUTOMATIC_CONTROL:
							qgc->servo_source(heli::MODE_AUTOMATIC_CONTROL);
							break;
						}
						break;
					}
					case UALBERTA_SET_CONTROL_MODE:
					{
						switch(action.param)
						{
						case UALBERTA_ATTITUDE_PID:
							qgc->control_mode(heli::Mode_Attitude_Stabilization_PID);
							break;
						case UALBERTA_TRANSLATION_PID:
							qgc->control_mode(heli::Mode_Position_Hold_PID);
							break;
						case UALBERTA_TRANSLATION_SBF:
							qgc->control_mode(heli::Mode_Position_Hold_SBF);
							break;
						}
						break;
					}
					case UALBERTA_SET_TRAJECTORY_TYPE:
					{
						switch(action.param)
						{
						case UALBERTA_POINT:
							Control::getInstance()->set_trajectory_type(heli::Point_Trajectory);
							break;
						case UALBERTA_LINE:
							Control::getInstance()->set_trajectory_type(heli::Line_Trajectory);
							break;
						case UALBERTA_CIRCLE:
							Control::getInstance()->set_trajectory_type(heli::Circle_Trajectory);
							break;
						}
						break;
					}
					case UALBERTA_SHUTDOWN:
					{
						qgc->message() << "QGCReceive: Received shutdown message from QGC.  Sending shutdown signal.";
						qgc->shutdown();
						break;
					}
					case UALBERTA_RESET_FILTER:
					{
						qgc->message() << "QGCReceive: Received filter reset message from QGC.  Sending filter reset signal";
						qgc->reset_filter();
						break;
					}
					case UALBERTA_INIT_ATTITUDE:
					{
						qgc->message("QGCReceive: Received filter init message from QGC. Sending attitude init signal");
						qgc->init_filter();
						break;
					}
					case UALBERTA_SET_ATTITUDE_SOURCE:
					{
						qgc->debug() << "Received change attitude source message with param " << static_cast<int>(action.param);
						if (action.param==UALBERTA_NAV_FILTER)
							qgc->attitude_source(true);
						else if (action.param == UALBERTA_AHRS)
							qgc->attitude_source(false);
						break;
					}
					case UALBERTA_SET_REF_POS:
					{
						qgc->debug("Received set reference position message");
						Control* control = Control::getInstance();
						control->set_reference_position();
						control->reset();
						break;
					}
					}
					break;
				}
				case MAVLINK_MSG_ID_REQUEST_DATA_STREAM:
				{
					qgc->debug("Request a Data Stream.");
					mavlink_request_data_stream_t m;
					mavlink_msg_request_data_stream_decode(&msg, &m);
					switch (m.req_stream_id)
					{
					case MAV_DATA_STREAM_RC_CHANNELS:
					{
						qgc->debug() << "RC Channel Stream at " << m.req_message_rate << " Hz.";
						qgc->set_rc_channel_rate(m.req_message_rate);

						break;
					}
					case MAV_DATA_STREAM_RAW_CONTROLLER:
					{
						qgc->debug() << "Controller Data Stream at " << m.req_message_rate << " Hz.";
						qgc->set_control_output_rate(m.req_message_rate);
						break;
					}
					case MAV_DATA_STREAM_POSITION:
					{
						qgc->debug() << "Position Data Stream at " << m.req_message_rate << " Hz.";
						qgc->set_position_rate(m.req_message_rate);
						break;
					}
					default:
					{
						qgc->debug("Unhandled Stream Requested");
						break;
					}
					}
				}
				case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:		// refresh parameter list, UAV must send parameters to QGC.
				{
					qgc->trace() << "received param request list.";
					CommonMessages::getInstance()->_sendParams = true;

					break;
				}
				break;
				case MAVLINK_MSG_ID_PARAM_SET:
				{
					qgc->trace() << "received param set.";
					mavlink_param_set_t set;
					mavlink_msg_param_set_decode(&msg, &set);

					if(((int)set.target_system) == qgc->uasId)
					{
						switch(set.target_component)
						{
						case heli::CONTROLLER_ID:
						{
							Control *control = Control::getInstance();
							control->setParameter(Parameter(std::string((const char*)(set.param_id)), set.param_value));
							std::vector<Parameter> plist = control->getParameters();
							for (std::vector<Parameter>::iterator it = plist.begin(); it != plist.end(); ++it)
							{
								if ((*it).getParamID() == (const char*)(set.param_id))
								{
									std::lock_guard<std::mutex> lock(qgc->requested_params_lock);
									qgc->requested_params.push(Parameter((*it).getParamID(), (*it).getValue(), heli::CONTROLLER_ID));
									qgc->debug() << __FILE__ << __LINE__ << "Sending Parameter: " << (*it);
								}
							}
							break;
						}
						case heli::HELICOPTER_ID:
						{
							Helicopter* bergen = Helicopter::getInstance();
							bergen->setParameter(Parameter(std::string((const char*)(set.param_id)), set.param_value));
							std::vector<Parameter> plist(bergen->getParameters());
							bool param_found = false;
							for (std::vector<Parameter>::iterator it = plist.begin(); it != plist.end() && !param_found; ++it)
							{
								if ((*it).getParamID() == (const char*)(set.param_id))
								{
									std::lock_guard<std::mutex> lock(qgc->requested_params_lock);
									qgc->requested_params.push(Parameter((*it).getParamID(), (*it).getValue(), heli::HELICOPTER_ID));
									qgc->debug() << __FILE__ << __LINE__ << "Sending Parameter: " << (*it);
									param_found = true;
								}
							}
							break;
						}
						default:
							qgc->warning("Received component id cannot be mapped to an on-board component.");
							break;
						}
					}
					break;
				}
				case MAVLINK_MSG_ID_PARAM_REQUEST_READ:		// read a single parameter on the list.
				{
					qgc->trace() << "QGCReceive: received param read request.";

					mavlink_param_request_read_t set;
					mavlink_msg_param_request_read_decode(&msg, &set);

					switch(set.target_component)
					{
					case heli::NAVFILTER_ID:
					{
//						NavFilter *nav = NavFilter::getInstance();
//						std::vector<Parameter> plist = nav->getParameters();
//						for (std::vector<Parameter>::iterator it = plist.begin(); it != plist.end(); ++it)
//						{
//							if ((*it).getParamID() == (const char*)(set.param_id))
//							{
//								std::lock_guard<std::mutex> lock(qgc->requested_params_lock);
//								qgc->requested_params.push(Parameter((*it).getParamID(), (*it).getValue(), heli::NAVFILTER_ID));
//							}
//						}
					break;
					}
					case heli::CONTROLLER_ID:
					{
						std::vector<Parameter> plist(Control::getInstance()->getParameters());

						qgc->trace() << "Searching for controller param with index: " << set.param_index << "from list size: " << plist.size();
						if (set.param_index >= 0)
						{
							uint index = set.param_index;
							if (index < plist.size())
							{
								qgc->trace() << "sending parameter: " << plist[index];
								qgc->requested_params.push(plist[index]);
							}
						}
						else
						{
							bool param_found = false;
							for (std::vector<Parameter>::iterator it = plist.begin(); (it != plist.end()) && (!param_found); ++it)
							{
								if (it->getParamID() == (const char*)(set.param_id))
								{
									std::lock_guard<std::mutex> lock(qgc->requested_params_lock);
									qgc->requested_params.push(Parameter(it->getParamID(), it->getValue(), heli::CONTROLLER_ID));
									param_found = true;
								}
							}
						}
						break;
					}
					case heli::HELICOPTER_ID:
					{
						std::vector<Parameter> plist(Helicopter::getInstance()->getParameters());
						bool param_found = false;
						for (std::vector<Parameter>::iterator it = plist.begin(); (it != plist.end()) && (!param_found); ++it)
						{
							if (it->getParamID() == (const char*)(set.param_id))
							{
								std::lock_guard<std::mutex> lock(qgc->requested_params_lock);
								qgc->requested_params.push(Parameter(it->getParamID(), it->getValue(), heli::HELICOPTER_ID));
								param_found = true;
							}
						}
						break;
					}
					}
				}
					break;
				#ifdef MAVLINK_ENABLED_UALBERTA
					case MAVLINK_MSG_ID_RADIO_CALIBRATION:
					{
						qgc->debug() << "QGCLink: got radio calibration packet";
						mavlink_radio_calibration_t radio_cal_msg;
						mavlink_msg_radio_calibration_decode(&msg, &radio_cal_msg);

						/* populate 3 setpoint servos */
						std::vector<uint16_t> aileron(3);
						std::vector<uint16_t> elevator(3);
						std::vector<uint16_t> rudder(3);
						for (int i=0; i<3; i++)
						{
							aileron[i] = radio_cal_msg.aileron[i];
							elevator[i] = radio_cal_msg.elevator[i];
							rudder[i] = radio_cal_msg.rudder[i];
						}

						/* populate 5 setpoint servos */
						std::vector<uint16_t> throttle(5);
						std::vector<uint16_t> pitch(5);
						for (int i=0; i<5; i++)
						{
							throttle[i] = radio_cal_msg.throttle[i];
							pitch[i] = radio_cal_msg.pitch[i];
						}

						/* populate switch */
						std::vector<uint16_t> gyro(2);
						for (int i=0; i<2; i++)
						{
							gyro[i] = radio_cal_msg.gyro[i];
						}

						/* set calibration */

						std::vector<std::vector<uint16_t> > calibration;
						calibration.push_back(aileron);
						calibration.push_back(elevator);
						calibration.push_back(throttle);
						calibration.push_back(rudder);
						calibration.push_back(gyro);
						calibration.push_back(pitch);
						RadioCalibration *cal = RadioCalibration::getInstance();
						cal->setCalibration(calibration);

						break;
					}
				#endif
				default:
                   qgc->debug() << "QGCLink: Unknown Packet: " << msg.msgid;
					break;
				}
			}
		}
	}
}
