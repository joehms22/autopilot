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

#include "QGCLink.h"

/* Project Headers */
#include "QGCReceive.h"
#include "QGCSend.h"
#include "Configuration.h"

#include <asio.hpp>

/* Mavlink Headers */
#include <mavlink.h>


// Configuration file name of the IP address param
const std::string QGCLINK_HOST_ADDRESS_PARAM = "qgroundcontrol.host.ip";
const std::string QGCLINK_HOST_ADDRESS_DEFAULT = "0.0.0.0";
const std::string QGCLINK_HOST_PORT_PARAM = "qgroundcontrol.host.port";
const int QGCLINK_HOST_PORT_DEFAULT = 14550;

// Function definitions

QGCLink::QGCLink()
: Driver("QGCLink", "qgroundcontrol"),
  socket(io_service),
  heartbeat_rate(10),
  position_rate(10),
  attitude_rate(10)
{
	configDescribe("UASidentifier",
                   "int",
                   "Unique numeric identifier for this system.");
	uasId = configGeti("UASidentifier", 100);

	init();
}

void QGCLink::init()
{
	try
	{
		Configuration* cfg = Configuration::getInstance();
		std::string ip_addr = cfg->gets(QGCLINK_HOST_ADDRESS_PARAM, QGCLINK_HOST_ADDRESS_DEFAULT);

		qgc.address(asio::ip::address::from_string(ip_addr));
		info() << "Opening socket to " << qgc.address().to_string();


		qgc.port(cfg->geti(QGCLINK_HOST_PORT_PARAM, QGCLINK_HOST_PORT_DEFAULT));

		// FIXME we didn't check to make sure the address is indeed IPV4 - Joseph
		socket.open(asio::ip::udp::v4());

		receive_thread = std::thread(QGCReceive());

        send_thread = std::thread([](){
            QGCSend::getInstance()->send();
        });
	}
	catch (std::exception& e)
	{
		critical() << e.what();
		throw e;
	}
}
