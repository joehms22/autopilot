/*******************************************************************************
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
 ******************************************************************************/

#ifndef DRIVER_H_
#define DRIVER_H_

#include <boost/thread.hpp>
#include <string.h>
#include <mutex>
#include <atomic>
#include "Debug.h"
#include "Configuration.h"


/**
 * The driver class is the base class for all the extension points in the program.
 *
 * @author Joseph Lewis <joehms22@gmail.com>
 */
class Driver : public Logger, public ConfigurationSubTree
{
private:
	/// store whether to terminate the thread
	std::atomic_bool _terminate;

	/// stores the prefix for the driver
	std::string _config_prefix;

	// Keeps the human readable name for the current driver.
	std::string _name;

	/// stores if the driver is going to do big debugging.
	bool _debug;

	// Keeps a list of all drivers so we can terminate them later.
	static std::mutex _all_drivers_lock;
	static std::list<Driver*> all_drivers;
	int _readDeviceType;


public:
	Driver(std::string name, std::string config_prefix);
	virtual ~Driver();
	static void terminateAll();
	inline const std::string getName(){return _name;};
	inline bool terminateRequested() {return _terminate;};
	inline void terminate() {
		debug() << "Driver Terminating: " << getName();
		_terminate = true;
	};

	/// Traces an item to the debugging output of the software if debugging has been set up in the config file.
	Debug trace();

	// Reads a fd in to the given buffer with a minimum of n bytes
	/**
	 * 
	 **/
	int readDevice(int fd, void * buf, int n);

	/**
	 * Sets the given terminal configuration on the given fd and saves them
	 * with name. If name already exists in the configuration file, those
	 * settings will overwrite these.
	 */
	bool namedTerminalSettings(std::string name,
									int fd,
									int baudrate,
									std::string parity,
									bool enableHwFlow,
									bool enableRawMode);


	/**
	 * Sets up the terminal at the given fd with the given baudrate,
	 * parity (one of: [8N1, 7E1, 701, 7M1, 7S1])
	 * hw flow control
	 * and raw mode
	 */
	bool terminalSettings(int fd,
									int baudrate,
									std::string parity,
									bool enableHwFlow,
									bool enableRawMode);

};

#endif /* DRIVER_H_ */
