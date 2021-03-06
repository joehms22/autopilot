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

#ifndef QGCSEND_H_
#define QGCSEND_H_

/* Project Headers */
#include "QGCLink.h"
#include "heli.h"
#include "IMU.h"

/* STL Headers */
#include <queue>
#include <vector>

/* Boost Headers */
#include <thread>
#include "Singleton.h"

/**
 * Send data over UDP to QGroundControl
 * @author Bryan Godbolt <godbolt@ece.ualberta.ca>
 *
 */
class QGCSend : public Singleton <QGCSend>
{
    friend Singleton<QGCSend>;

public:

    /** queue stream messages and perform actual send */
	void send();

	/// threadsafe push message to the console
	inline void message_queue_push(const std::string& message) {std::lock_guard<std::mutex> lock(message_queue_lock); message_queue.push(message);}

private:
    QGCSend();
    ~QGCSend();

	/// Pointer to QGCLink instance
	QGCLink *qgc;

	/// queue to store the message to be sent
	std::queue<std::vector<uint8_t> > *send_queue;


	/** queue up a heartbeat message
	 * @param sendq queue to put heartbeat message in */
	void send_heartbeat(std::queue<std::vector<uint8_t> >* sendq);
	/** queue up a raw imu sense message
	 * @param sendq queue to put message in
	 */
	void send_raw_imu(std::queue<std::vector<uint8_t> >* sendq);
	/** queue up status message
	 * @param sendq queue to put messages in
	 */
	void send_status(std::queue<std::vector<uint8_t> >* sendq);
	/** queue up attitude estimate message
	 * @param sendq queue to put messages in
	 */
	void send_attitude(std::queue<std::vector<uint8_t> > *sendq);
	/// send the current position and velocity measurement
	void send_position(std::queue<std::vector<uint8_t> > *sendq);
	/** add console message to send queue
     * @param message - the message to send
	 * @param sendq send queue
	 */
	void send_console_message(const std::string& message, std::queue<std::vector<uint8_t> > *sendq);

	/** determine whether to send a particular stream
	 * @param stream_rate requested stream rate
	 * @param send_rate rate at which the send loop runs
	 * @param count number of itereates of the send loop
	 * @returns true if stream should be send at this iteration, false if not.
	 */
	bool should_run(int stream_rate, int send_rate, int count);

	/// store the current servo source
	std::atomic<heli::AUTOPILOT_MODE> servo_source;
	inline heli::AUTOPILOT_MODE get_servo_source() const {return servo_source;}
	inline void set_servo_source(heli::AUTOPILOT_MODE servo_source) {this->servo_source = servo_source;}

	/// store the current pilot mode (threadsafe)
	std::atomic<heli::PILOT_MODE> pilot_mode;
	inline heli::PILOT_MODE get_pilot_mode() const {return pilot_mode;}
	inline void set_pilot_mode(heli::PILOT_MODE pilot_mode) {this->pilot_mode = pilot_mode;}

	/// store the current gx3 filter state
	std::atomic<IMU::GX3_MODE> filter_state;
	inline IMU::GX3_MODE get_filter_state() const {return filter_state;}
	inline void set_filter_state(IMU::GX3_MODE filter_state) {this->filter_state = filter_state;}

	/// store the current controller mode
	std::atomic<heli::Controller_Mode> control_mode;
	inline heli::Controller_Mode get_control_mode() const {return control_mode;}
	inline void set_control_mode(heli::Controller_Mode control_mode) {this->control_mode = control_mode;}

	/// store the current attitude source (threadsafe)
	std::atomic_bool attitude_source;
	inline bool get_attitude_source() const {return attitude_source;}
	inline void set_attitude_source(bool source) {attitude_source = source;}
	/// connection for attitude source
	boost::signals2::scoped_connection attitude_source_connection;

	/// store a queue of messages
	std::queue<std::string> message_queue;
	/// serialize access to message_queue
	mutable std::mutex message_queue_lock;
	/// threadsafe empty test
	inline bool message_queue_empty() const {std::lock_guard<std::mutex> lock(message_queue_lock); return message_queue.empty();}
	/// threadsafe pop message
	std::string message_queue_pop();
};
#endif
