/**
 * @file
 * @brief Implementation of Caribou Satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CaribouSatellite.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

// Caribou Peary includes
#include <peary/device/DeviceManager.hpp>
#include <peary/utils/configuration.hpp>
#include <peary/utils/exceptions.hpp>
#include <peary/utils/logging.hpp>

#include "constellation/core/logging/log.hpp"
#include "constellation/satellite/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace caribou;
using namespace constellation::satellite;
using namespace std::literals::chrono_literals;

CaribouSatellite::CaribouSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), manager_(std::make_shared<DeviceManager>()) {

    // Custom Caribou commands for this satellite:

    register_command(
        "peary_verbosity", "Set verbosity of the Peary logger.", {}, std::function<void(const std::string&)>([](auto level) {
            caribou::Log::setReportingLevel(caribou::Log::getLevelFromString(level));
        }));
    register_command("list_registers",
                     "List all available register names for the attached Caribou device.",
                     {State::INIT, State::ORBIT, State::RUN},
                     std::function<std::vector<std::string>()>([&]() {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->listRegisters();
                     }));
    register_command("list_memories",
                     "List all memory registers for the attached Caribou device.",
                     {State::INIT, State::ORBIT, State::RUN},
                     std::function<std::vector<std::string>()>([&]() {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->listMemories();
                     }));
    register_command("get_voltage",
                     "Get selected output voltage (in V) of the attached Caribou device. Provide voltage name as parameter.",
                     {State::INIT, State::ORBIT, State::RUN},
                     std::function<double(const std::string&)>([&](auto name) {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->getVoltage(name);
                     }));
    register_command("get_current",
                     "Get selected output current (in A) of the attached Caribou device. Provide current name as parameter.",
                     {State::INIT, State::ORBIT, State::RUN},
                     std::function<double(const std::string&)>([&](auto name) {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->getCurrent(name);
                     }));
    register_command("get_power",
                     "Get selected output power (in W) of the attached Caribou device. Provide power name as parameter.",
                     {State::INIT, State::ORBIT, State::RUN},
                     std::function<double(const std::string&)>([&](auto name) {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->getPower(name);
                     }));
    register_command("get_register",
                     "Read the value of register on the attached Caribou device. Provide register name as parameter.",
                     {State::INIT, State::ORBIT, State::RUN},
                     std::function<uintptr_t(const std::string&)>([&](auto name) {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->getRegister(name);
                     }));
    register_command("get_memory",
                     "Read the value of FPGA memory register on the attached Caribou device. Provide memory register "
                     "name as parameter.",
                     {State::INIT, State::ORBIT, State::RUN},
                     std::function<uintptr_t(const std::string&)>([&](auto name) {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->getMemory(name);
                     }));
    register_command("get_adc",
                     "Read the voltage from the ADC voltage NAME (in V) via the attached Caribou device. Provide the "
                     "voltage name as string.",
                     {State::INIT, State::ORBIT, State::RUN},
                     std::function<double(const std::string&)>([&](auto name) {
                         std::lock_guard<std::mutex> lock {device_mutex_};
                         return device_->getADC(name);
                     }));
}

void CaribouSatellite::initializing(constellation::config::Configuration& config) {

    // Set default values:
    config.setDefault("adc_frequency", 1000);

    // Clear all existing devices - the initializing method can be called multiple times!
    manager_->clearDevices();

    // Read the device type from the configuration:
    device_class_ = config.get<std::string>("type");
    LOG(INFO) << "Instantiated " << getCanonicalName() << " for device \"" << device_class_ << "\"";

    // Open configuration file and read caribou configuration
    const auto config_file_path = config.getPath("config_file");
    LOG(INFO) << "Attempting to use initial device configuration " << config_file_path;
    std::ifstream config_file {config_file_path};
    if(!config_file.is_open()) {
        throw SatelliteError("Could not open configuration file \"" + config_file_path.string() + "\"");
    }
    auto caribou_config = caribou::Configuration(config_file);

    // Select section from the configuration file relevant for this device
    const auto sections = caribou_config.GetSections();
    if(std::find(sections.cbegin(), sections.cend(), device_class_) == sections.cend()) {
        throw SatelliteError("Could not find section for device \"" + device_class_ + "\" in config file \"" +
                             config_file_path.string() + "\"");
    }
    caribou_config.SetSection(device_class_);

    std::lock_guard<std::mutex> lock {device_mutex_};
    try {
        const auto device_id = manager_->addDevice(device_class_, caribou_config);
        LOG(INFO) << "Manager returned device ID " << device_id << ", fetching device...";
        device_ = manager_->getDevice(device_id);
    } catch(const caribou::DeviceException& error) {
        throw SatelliteError("Failed to get device \"" + device_class_ + "\": " + error.what());
    }

    // Add secondary device if it is configured
    if(config.has("secondary_device")) {
        const auto secondary = config.get<std::string>("secondary_device");
        if(std::find(sections.begin(), sections.end(), secondary) != sections.end()) {
            caribou_config.SetSection(secondary);
        }
        try {
            const auto device_id2 = manager_->addDevice(secondary, caribou_config);
            LOG(INFO) << "Manager returned device ID " << device_id2 << ", fetching secondary device...";
            secondary_device_ = manager_->getDevice(device_id2);
        } catch(const caribou::DeviceException& error) {
            throw SatelliteError("Failed to get secondary device \"" + secondary + "\": " + error.what());
        }
    }

    LOG(STATUS) << getCanonicalName() << " initialized";
}

void CaribouSatellite::launching() {
    LOG(INFO) << "Configuring device " << device_->getName();

    std::lock_guard<std::mutex> lock {device_mutex_};

    // Switch on the device power
    device_->powerOn();

    if(secondary_device_ != nullptr) {
        secondary_device_->powerOn();
    }

    // Wait for power to stabilize and for the TLU clock to be present
    std::this_thread::sleep_for(1s);

    // Configure the device
    device_->configure();

    if(secondary_device_ != nullptr) {
        secondary_device_->configure();
    }

    // Set additional registers from the configuration:
    const auto& config = getConfig();
    if(config.has("register_key") && config.has("register_value")) {
        auto key = config.get<std::string>("register_key");
        auto value = config.get<uintptr_t>("register_value");
        device_->setRegister(key, value);
        LOG(INFO) << "Setting " << key << " = " << std::to_string(value);
    }

    if(config.has("adc_signal")) {
        // Select which ADC signal to regularly fetch:
        adc_signal_ = config.get<std::string>("adc_signal");
        adc_freq_ = config.get<std::uint64_t>("adc_frequency");

        // Try it out directly to catch mis-configuration
        auto adc_value = device_->getADC(adc_signal_);
        LOG(INFO) << "Will probe ADC signal \"" << adc_signal_ << "\" every " << adc_freq_ << " frames";
        LOG(TRACE) << "ADC value: " << adc_value; // FIXME: unused variable, send as stats instead
    }

    LOG(STATUS) << getCanonicalName() << " launched";
}

void CaribouSatellite::landing() {
    LOG(INFO) << "Switching off power for device " << device_->getName();

    // Switch off the device power
    std::lock_guard<std::mutex> lock {device_mutex_};
    device_->powerOff();

    LOG(STATUS) << getCanonicalName() << " landed";
}

void CaribouSatellite::reconfiguring(const constellation::config::Configuration& /*partial_config*/) {}

void CaribouSatellite::starting(std::string_view run_identifier) {
    LOG(INFO) << "Starting run " << run_identifier << "...";

    // Reset frame number
    frame_nr_ = 0;

    // Start the DAQ
    std::lock_guard<std::mutex> lock {device_mutex_};

    // How to add additional information for the Begin-of-run event, e.g. containing tags with detector information?
    /*DataSender::BORMessage bor_msg {};
    bor_msg.add_tag("software", device_->getVersion());
    bor_msg.add_tag("firmware", device_->getFirmwareVersion());
    const auto registers = device_->getRegisters();
    for(const auto& reg : registers) {
        bor_msg.add_tag(reg.first, reg.second);
    }
    data_sender_.sendBOR(std::move(bor_msg));*/

    // Start DAQ:
    device_->daqStart();

    LOG(STATUS) << getCanonicalName() << " started (run " << run_identifier << ")";
}

void CaribouSatellite::stopping() {
    LOG(INFO) << "Stopping run...";

    // Stop the DAQ
    std::lock_guard<std::mutex> lock {device_mutex_};
    device_->daqStop();

    LOG(STATUS) << getCanonicalName() << " stopped";
}

void CaribouSatellite::running(const std::stop_token& stop_token) {
    LOG(INFO) << "Starting run loop...";

    std::lock_guard<std::mutex> lock {device_mutex_};

    while(!stop_token.stop_requested()) {
        try {
            // Retrieve data from the device
            LOG(TRACE) << "Trying to receive data from device";
            const auto data = device_->getRawData();

            LOG(DEBUG) << "Frame " << frame_nr_;

            if(!data.empty()) {
                // Create new data message for data sender
                /*DataSender::Message msg {};

                // Set frame number
                msg.addTag("frame", frame_nr_);

                // Add data
                msg.addDataFrame(std::move(data));

                // Query ADC if wanted:
                if(frame_nr_ % adc_freq_ == 0) {
                    if(!adc_signal_.empty()) {
                        auto adc_value = device_->getADC(adc_signal_);
                        LOG(DEBUG) << "ADC reading: " << adc_signal_ << " =  " << std::to_string(adc_value);
                        msg.addTag(adc_signal_, adc_value);
                    }
                }

                // Send data (async)
                data_sender_.sendData(std::move(msg));*/
            }

            // Now increment the frame number
            frame_nr_++;
        } catch(caribou::NoDataAvailable&) {
            continue;
        } catch(caribou::DataException& e) {
            // Retrieval failed, retry once more before aborting:
            LOG(WARNING) << e.what() << ", skipping data packet";
            continue;
        } catch(caribou::caribouException& e) {
            // FIXME sat exceptions missing.
            LOG(CRITICAL) << e.what();
            throw std::exception();
        }
    }

    LOG(INFO) << "Exiting run loop";
}