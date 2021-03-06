/*******************************************************************************
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
 ******************************************************************************/

#ifndef LOGFILE_H
#define LOGFILE_H

/* STL Headers */
#include <sstream>
#include <exception>
#include <string>
#include <chrono>
#include <mutex>
#include <atomic>

/* c headers */
#include <stdint.h>
#include <time.h>

#include "Driver.h"
#include "ThreadSafeVariable.h"
#include "Singleton.h"
#include "Path.h"

/**
   \brief This class implements the logging facility for the avionics.

   \author Bryan Godbolt <godbolt@ece.ualberta.ca>
   \date March 1, 2011: Creation of class
   \date June 18, 2011: Add separate writing thread
   @date October 20, 2011: Add std::array log function

   This class uses the singleton design pattern to enforce the user to only
   instantiate one object.  In order to get the instance of the class you can
   declare a variable as follows,
   \code
   LogFile *log = LogFile::getInstance();
   \endcode
   Once you have a pointer to the instance of the LogFile you can just add
   vectors to a log of any type.  There is no further initialization necessary.
   \code
   vector<float> data_vector;
   // put data in data_vector for current iteration
   log->logData("file name", data_vector);
   \endcode
   The string passed as filename internally identifies a queue where the vector
   is stored.  Every time you call LogFile::logData() the data vector is appended
   to the queue identified by the file name string.

   \warning The data type of the vector passed to LogFile::logData() must implement
   the \code ostream& operator<<(ostream&, T&) \endcode
   operator, where T refers the type of the data vector.
   \note This operator is defined for all basic types (e.g., int, float, etc)

   Each file can optionally be assigned a header string using LogFile::logHeader()
   which will be written out at the top of the log file.  LogFile::logHeader() must be
   called before the first call to LogFile::logData for a particular file.

	The data write is performed in a seperate thread.  This means when the program is going to terminate
	the LogFile object (or more precisely the LogFileWrite object) must be allow to
	finish writing any data it has in its buffer which has not yet been written.
	Currently this functionality is implemented by MainApp::terminate which sends a signal
	which is received by LogFileWrite when the program is about to exit, and by MainApp::add_thread()
	which allows LogFileWrite to tell MainApp it exists so that the main program can wait
	until LogFileWrite terminates before exiting.

   \code
	using std::vector;

	vector<float> norms(8);

	LogFile *log = LogFile::getInstance();


	log->logHeader("normalized outputs", "CH1(us)\tCH2(us)\tCH3(us)\tCH4(us)\tCH5(us)\tCH6(us)\tCH7(us)\tCH8(us)");
	for (int i=0; i<10; i++)
	{
		for(float & norm : norms)
		{
			norm = rand() % 1000 + 1000;
		}
		log->logData("normalized outputs", norms);
	}
   \endcode

   \todo Add the ability to set a base directory
 */

class LogFile : public Singleton<LogFile>
{
    friend Singleton<LogFile>;

public:

    /**
       Assigns a header to a log file
       \param name log file to assign a name to
       \param header string to be written at the head of the log file
       This function must be called before the first call to logData in order to have an effect.
    */
    void logHeader(const std::string& name, const std::string& header);

    /**
     * Template log function which logs any data container that supports
     * const iterators
     */
    template<typename DataContainer>
    void logData(const std::string& name, const DataContainer& data);
    /**
     * Allows message logging by appending msg to the log, name
     * @param name log file to append message to
     * @param msg message to append to name
     */
    void logMessage(const std::string& name, const std::string& msg);

    /**
     * Start a new set of log files with current date/time
     */
    void newLogPoint();

    Path getLogFolder()
    {
        std::lock_guard<std::mutex> lg(_logFolderLock);
        return log_folder;
    }

private:

    long getMicrosSinceInit()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now() - startTime).count();
    }

    /// Singleton constructor: allocates memory for internal data structures
    LogFile();

    /// Destructor: frees memory used by the internal data structures
    ~LogFile();

    void setupLogFolder();

    /// Stores the time when the class is instantiated (i.e., the program starts)
    std::chrono::time_point<std::chrono::system_clock> startTime;
    /// Stores the folder name to store the log files in
    Path log_folder;

    /// Stores the log checkpoint for the current file.
    std::atomic<int> _checkpoint;

    /// The lock for the log folder.
    std::mutex _logFolderLock;
};


template<typename DataContainer>
void LogFile::logData(const std::string& name, const DataContainer& data)
{
    std::stringstream output;

    for (typename DataContainer::const_iterator it = data.begin(); it != data.end(); ++it)
    {
        output << std::to_string(*it);
        output << '\t';
    }

    logMessage(name, output.str()); // adds timestamp, endline
};


#endif
