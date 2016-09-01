/*
 * Copyright 2016 OSVR and contributors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

// Internal Includes
#include <osvr/PluginKit/PluginKit.h>
#include <osvr/PluginKit/AnalogInterfaceC.h>

// Generated JSON header file
#include "com_osvr_user_settings_json.h"

// Library/third-party includes
// - none

// Standard includes
#include <iostream>
#include <fstream>
#include <vector>

// set up for file watching
#include <stdlib.h>
#include <stdio.h>
#include <libfswatch/c++/monitor.hpp>

#include "../osvruser.h"

struct Constants{
	static string config_file;
	static string config_path;
};

string Constants::config_file = "osvr_user_settings.json";
string Constants::config_path = "C:/ProgramData/OSVR/";

// namespace to avoid symbol collision
namespace usersettingsplugin {
void filewatcher_cb(const std::vector<fsw::event>& events, void *data);
class AnalogSyncDevice {
  public:
    AnalogSyncDevice(OSVR_PluginRegContext ctx) : m_myVal(0) {

		// This prints the environment variable value
#ifdef _WIN32
        //TODO: fix windows part
		TCHAR Variable[MAX_PATH];
		GetEnvironmentVariable(_T("PROGRAMDATA"), Variable, MAX_PATH);
		Constants::config_path = Variable;
		Constants::config_path += +L"\\OSVR\\";

		wstring ss = Constants::config_path + Constants::config_file;
#else
                char* xdgpath = std::getenv("XDG_CONFIG_HOME");
                if (xdgpath) {
                    Constants::config_path = std::string(xdgpath);
                } else {
                    string username = std::getenv("USER");
                    Constants::config_path = "/home/" + username + "/.config";
                }
                string ss = Constants::config_path + "/" + Constants::config_file;
                std::cout << "UserSettings: Using config file " << ss << std::endl;
#endif

		readConfigFile(ss);
                std::vector<std::string> files;
                files.push_back(ss);
                active_monitor = fsw::monitor_factory::create_monitor(fsw_monitor_type::system_default_monitor_type, files, &filewatcher_cb);
                active_monitor->start();
		long Result = 0; //m_FileWatcher.addPath(path);
		if (Result == 0){
			std::cout << "UserSettings: file watch on " << string(ss.begin(),ss.end()) << " setup." << std::endl;
		}
		else{
			std::cout << Result << "UserSettings: file watch on " << string(ss.begin(), ss.end()) << " failed." << std::endl;
		}

		/// Create the initialization options
        OSVR_DeviceInitOptions opts = osvrDeviceCreateInitOptions(ctx);

        /// Indicate that we'll want 3 analog channels.
        osvrDeviceAnalogConfigure(opts, &m_analog, 4);

        /// Create the sync device token with the options
        m_dev.initSync(ctx, "UserSettings", opts);

        /// Send JSON descriptor
        m_dev.sendJsonDescriptor(
            com_osvr_user_settings_json);

        /// Register update callback
        m_dev.registerUpdateCallback(this);
	};

	void readConfigFile(string file_locator){

		std::ifstream file_id;
		file_id.open(file_locator);

		Json::Reader reader;
		Json::Value value;
		if (!reader.parse(file_id, value)){
			std::cout << "USER_SETTINGS_PLUGIN: Couldn't open save file, creating file.\n";
			// close file first just in case...
			file_id.close();
			// new file just has default values
			writeConfigFile(file_locator);
		}
		else{
			m_osvrUser.read(value);
			file_id.close();
		}
	};

	void writeConfigFile(string file_locator){
		// retreive Json string. should be just the default values.
		Json::Value value;
		m_osvrUser.write(value);

		// open the file
		std::ofstream file_id;
		file_id.open(file_locator);

		// write the json structure to the file
		Json::StyledWriter styledWriter;
		file_id << styledWriter.write(value);
		file_id.close();
	};

	OSVR_ReturnCode update() {

		if (m_fileChange){
			std::cout << "UserSettings: file changed..." << std::endl;
			m_fileChange = 0;
			readConfigFile(Constants::config_path + Constants::config_file);
		}

		OSVR_AnalogState values[4];

		values[0] = m_osvrUser.pupilDistance(OS) + m_osvrUser.pupilDistance(OD);
		values[1] = m_osvrUser.standingEyeHeight();
		values[2] = m_osvrUser.seatedEyeHeight();
		values[3] = rand();
		osvrDeviceAnalogSetValues(m_dev, m_analog, values, 4);

		return OSVR_RETURN_SUCCESS;
		};

	long m_fileChange = 0;

  private:
	OSVRUser m_osvrUser;
	osvr::pluginkit::DeviceToken m_dev;
    OSVR_AnalogDeviceInterface m_analog;
    fsw::monitor *active_monitor;
	double m_myVal;
	bool m_initialized = false;

};

//TODO: Get this into the AnalogSyncDevice class
AnalogSyncDevice *asd = NULL;
void filewatcher_cb(const std::vector<fsw::event>& events,
                                      void *data) {
    //std::cout << "Callback" << std::endl;
    for (fsw::event e : events) {
        
        if (e.get_path() != "" /*whyever this happens*/ && e.get_event_flag_by_name("Updated")) { //TODO: Does it work this way?
            std::cout << e.get_path() << " changed..." << std::endl;
            if (asd != NULL) {
                asd->update();
            }
        } else {
            //std::cout << "foo" << std::endl;
        }
    }
}

class HardwareDetection {
  public:
    HardwareDetection() : m_found(false) {}
    OSVR_ReturnCode operator()(OSVR_PluginRegContext ctx) {

        std::cout << "UserSettings: plugin instantiated" << std::endl;
        if (!m_found) {
            std::cout << "UserSettings: Reading settings file" << std::endl;
            m_found = true;

            /// Create our device object
            asd = new AnalogSyncDevice(ctx);
            osvr::pluginkit::registerObjectForDeletion(
                ctx, asd);
        }
        return OSVR_RETURN_SUCCESS;
    }

  private:
    /// @brief Have we found our device yet? (this limits the plugin to one
    /// instance)
    bool m_found;
};
} // namespace

OSVR_PLUGIN(com_osvr_user_settings) {
    osvr::pluginkit::PluginContext context(ctx);

    /// Register a detection callback function object.
    context.registerHardwareDetectCallback(new usersettingsplugin::HardwareDetection());

	/// Set up a file change notify to trigger re-reading of file
//	Watcher::run();

	return OSVR_RETURN_SUCCESS;
}
