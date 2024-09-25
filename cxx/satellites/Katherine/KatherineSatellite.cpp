/**
 * @file
 * @brief Implementation of the Katherine satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "KatherineSatellite.hpp"

#include <condition_variable>
#include <fstream>
#include <mutex>
#include <string_view>

#include "constellation/core/log/log.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

using namespace constellation::config;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;

KatherineSatellite::KatherineSatellite(std::string_view type, std::string_view name) : TransmitterSatellite(type, name) {
    register_command("get_hw_info",
                     "Read hardware revision and other information from the device.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     &KatherineSatellite::get_hw_info,
                     this);
    register_command("get_link_status",
                     "Read chip communication link status from the device.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<std::vector<std::string>()>([&]() -> std::vector<std::string> {
                         std::lock_guard<std::mutex> lock {katherine_cmd_mutex_};
                         auto state = device_->comm_status();
                         return {"Line mask " + char_to_hex_string(state.comm_lines_mask),
                                 "Data rate " + to_string(state.data_rate),
                                 (state.chip_detected ? "Chip present" : "Chip absent")};
                     }));
    register_command("get_temperature_readout",
                     "Read the current temperature from the Katherine readout board.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<double()>([&]() {
                         std::lock_guard<std::mutex> lock {katherine_cmd_mutex_};
                         return device_->readout_temperature();
                     }));
    register_command("get_temperature_sensor",
                     "Read the current temperature from the temperature sensor.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<double()>([&]() {
                         std::lock_guard<std::mutex> lock {katherine_cmd_mutex_};
                         return device_->sensor_temperature();
                     }));
    register_command("get_adc_voltage",
                     "Read the voltage from the ADC channel provided as parameter.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<double(std::uint8_t)>([&](auto channel) {
                         std::lock_guard<std::mutex> lock {katherine_cmd_mutex_};
                         return device_->adc_voltage(channel);
                     }));
    register_command("get_chip_id",
                     "Read the chip ID of the attached sensor.",
                     {CSCP::State::INIT, CSCP::State::ORBIT, CSCP::State::RUN},
                     std::function<std::string()>([&]() {
                         std::lock_guard<std::mutex> lock {katherine_cmd_mutex_};
                         return device_->chip_id();
                     }));
}

void KatherineSatellite::initializing(constellation::config::Configuration& config) {

    // Set default values for the configuration:
    config.setDefault("positive_polarity", true);
    config.setDefault("sequential_mode", false);
    config.setDefault("op_mode", OperationMode::TOA_TOT);
    config.setDefault("shutter_mode", ShutterMode::AUTO);
    config.setDefault("data_buffer", 34952533);
    config.setDefault("pixel_buffer", 65536);
    config.setDefault("decode_data", true);

    auto ip_address = config.get<std::string>("ip_address");
    LOG(DEBUG) << "Attempting to connect to Katherine system at " << ip_address;

    try {
        const std::lock_guard device_lock {katherine_cmd_mutex_};
        const std::lock_guard acquisition_lock {katherine_acq_mutex_};
        const std::lock_guard data_lock {katherine_data_mutex_};

        // If we already have a device connected, remove it - we might be calling initialize multiple times!
        if(device_) {
            // Calls katherine_device_fini which closes CTRL & DATA UDP sockets
            device_.reset();
        }

        // Connect to Katherine system, print device ID
        // Calls katherine_device_init which sets up UDP sockets for CTRL & DATA
        device_ = std::make_shared<katherine::device>(ip_address);

        // Read back information
        LOG(STATUS) << "Connected to Katherine at " << ip_address;
        LOG(STATUS) << "    Current board temperature: " << device_->readout_temperature() << "C"; // CTRL UDP socket
        LOG(STATUS) << "    Current sensor temperature: " << device_->sensor_temperature() << "C"; // CTRL UDP socket

        // Check that chip is connected:
        const auto link_status = device_->comm_status();
        if(!link_status.chip_detected) {
            throw CommunicationError("No chip detected at Katherine system");
        }
        LOG(STATUS) << "    Chip detected, link speed " << to_string(link_status.data_rate);

        // Cross-check Chip ID if provided
        const auto chip_id = device_->chip_id(); // CTRL UDP socket
        if(config.has("chip_id") && config.get<std::string>("chip_id") != chip_id) {
            throw InvalidValueError(config, "chip_id", "Invalid chip ID, system reports " + chip_id);
        }
        LOG(STATUS) << "    Reported chip ID: " << device_->chip_id();
    } catch(katherine::system_error& error) {
        throw CommunicationError("Katherine error: " + std::string(error.what()));
    }

    LOG(DEBUG) << "Configuring Katherine system";
    katherine_config_ = katherine::config {};

    // Set datadriven or framebased mode:
    ro_type_ =
        (config.get<bool>("sequential_mode") ? katherine::readout_type::sequential : katherine::readout_type::data_driven);
    opmode_ = config.get<OperationMode>("op_mode");

    // Set threshold polarity
    katherine_config_.set_polarity_holes(config.get<bool>("positive_polarity"));

    // Trigger configuration
    auto trigger_mode = config.get<ShutterMode>("shutter_mode");
    LOG(INFO) << "Configured trigger mode to " << to_string(trigger_mode);

    if(trigger_mode == ShutterMode::AUTO) {
        katherine_config_.set_start_trigger(katherine::no_trigger);
    } else {
        // No "autotriggering"
        // enabled, channel, use_falling_edge
        katherine_config_.set_start_trigger(
            {true, 0, (trigger_mode == ShutterMode::POS_EXT || trigger_mode == ShutterMode::POS_EXT_TIMER) ? false : true});
    }

    if(trigger_mode == ShutterMode::POS_EXT_TIMER || trigger_mode == ShutterMode::NEG_EXT_TIMER) {
        auto trig_width = std::chrono::nanoseconds(config.get<std::uint64_t>("shutter_width"));
        LOG(INFO) << "Shutter length: " << trig_width;
        katherine_config_.set_acq_time(trig_width);
    }

    katherine_config_.set_stop_trigger(katherine::no_trigger);
    katherine_config_.set_delayed_start(false);

    // For now, these constants are hard-coded.
    LOG(DEBUG) << "Configuring bias, clocks and frame setup";
    katherine_config_.set_bias_id(0);
    katherine_config_.set_bias(0);

    // Set number of frames to acquire
    katherine_config_.set_no_frames(config.get<int>("no_frames", 1));
    if(ro_type_ == katherine::readout_type::data_driven && katherine_config_.no_frames() > 1) {
        throw InvalidValueError(config, "no_frames", "Data-driven mode requires a single frame");
    }

    katherine_config_.set_gray_disable(false);
    katherine_config_.set_phase(katherine::phase::p1);
    katherine_config_.set_freq(katherine::freq::f40);

    // Set the DACs in the Katherine config
    auto dacs = parse_dacs_file(config.getPath("dacs_file"));
    LOG(DEBUG) << "Sending DACs to Katherine system";
    katherine_config_.set_dacs(std::move(dacs));

    auto px_config = parse_px_config_file(config.getPath("px_config_file"));
    katherine_config_.set_pixel_config(std::move(px_config));

    // Set how many pixels are buffered before returning and sending a message
    data_buffer_depth_ = config.get<int>("data_buffer");
    pixel_buffer_depth_ = config.get<int>("pixel_buffer");
    decode_data_ = config.get<bool>("decode_data");
}

void KatherineSatellite::launching() {
    // If we are in data-driven mode, disable timeout:
    auto timeout = (ro_type_ == katherine::readout_type::data_driven) ? -1s : 10s;

    // Lock the data/acquisition mutex
    const std::lock_guard acquisition_lock {katherine_acq_mutex_};

    // Select acquisition mode and create acquisition object
    // The acquisition object constructor calls katherine_acq_init which
    // - allocates data buffers
    // - stores device reference
    // - set handlers
    // There does not appear to be any UDP communication ongoing.
    if(opmode_ == OperationMode::TOA_TOT) {
        auto acq = std::make_shared<katherine::acquisition<katherine::acq::f_toa_tot>>(
            *device_,
            katherine::md_size * data_buffer_depth_,
            sizeof(katherine::acq::f_toa_tot::pixel_type) * pixel_buffer_depth_,
            500ms,
            timeout,
            decode_data_);
        acq->set_pixels_received_handler(
            std::bind_front(&KatherineSatellite::pixels_received<katherine::acq::f_toa_tot::pixel_type>, this));
        acquisition_ = acq;
    } else if(opmode_ == OperationMode::TOA) {
        auto acq = std::make_shared<katherine::acquisition<katherine::acq::f_toa_only>>(
            *device_,
            katherine::md_size * data_buffer_depth_,
            sizeof(katherine::acq::f_toa_only::pixel_type) * pixel_buffer_depth_,
            500ms,
            timeout,
            decode_data_);
        acq->set_pixels_received_handler(
            std::bind_front(&KatherineSatellite::pixels_received<katherine::acq::f_toa_only::pixel_type>, this));
        acquisition_ = acq;
    } else if(opmode_ == OperationMode::EVT_ITOT) {
        auto acq = std::make_shared<katherine::acquisition<katherine::acq::f_event_itot>>(
            *device_,
            katherine::md_size * data_buffer_depth_,
            sizeof(katherine::acq::f_event_itot::pixel_type) * pixel_buffer_depth_,
            500ms,
            timeout,
            decode_data_);
        acq->set_pixels_received_handler(
            std::bind_front(&KatherineSatellite::pixels_received<katherine::acq::f_event_itot::pixel_type>, this));
        acquisition_ = acq;
    }

    acquisition_->set_frame_started_handler(std::bind_front(&KatherineSatellite::frame_started, this));
    acquisition_->set_frame_ended_handler(std::bind_front(&KatherineSatellite::frame_ended, this));
    acquisition_->set_data_received_handler(std::bind_front(&KatherineSatellite::data_received, this));
}

void KatherineSatellite::data_received(const char* data, size_t count) {
    auto msg = newDataMessage();
    LOG(TRACE) << "Received buffer with " << count << " words";

    if(count % 6 != 0) {
        std::string msg = "Number of data words doesn't match measurement data granularity";
        LOG(CRITICAL) << msg;
        throw CommunicationError(msg);
    }

    // Measurement data us 6 bytes, pack each in a frame
    for(size_t i = 0; i < count; i += 6) {
        msg.addFrame(std::string(data + i, data + i + 6));
    }
    // Try to send and retry if it failed:
    LOG(DEBUG) << "Sending message with " << msg.countFrames() << " pixels";
    trySendDataMessage(msg);
}

void KatherineSatellite::landing() {
    if(acquisition_) {
        const std::lock_guard acquisition_lock {katherine_acq_mutex_};
        // Calls katherine_acq_fini which frees data buffers
        acquisition_.reset();
    }
}

void KatherineSatellite::interrupting(CSCP::State) {
    if(acquisition_) {
        const std::lock_guard acquisition_lock {katherine_acq_mutex_};

        // Read the current acquisition state:
        if(!acquisition_->aborted()) {
            const std::lock_guard device_lock {katherine_cmd_mutex_};
            // Calls katherine_acq_abort which sends the stop command via CTRL UDP
            acquisition_->abort();
        }

        // Wait for acquisition task to finish
        if(acq_future_.valid()) {
            acq_future_.wait();
            acq_future_.get();
        }

        // Calls katherine_acq_fini which frees data buffers
        acquisition_.reset();
    }
}

void KatherineSatellite::failure(CSCP::State state) {
    // Currently same as interrupting - what happens if we lose comm with Katherine system?
    interrupting(state);
}

void KatherineSatellite::frame_started(int frame_idx) {
    LOG(INFO) << "Started frame " << frame_idx << std::endl;
}

void KatherineSatellite::frame_ended(int frame_idx, bool completed, const katherine_frame_info_t& info) {
    LOG(STATUS) << "Frame " << frame_idx << " finished, started at " << info.start_time.d << ", ended at "
                << info.end_time.d;
    LOG_IF(WARNING, info.lost_pixels > 0) << "TPX3 -> Katherine lost " << info.lost_pixels << " pixels";
    LOG_IF(WARNING, info.sent_pixels > info.received_pixels)
        << "Katherine -> PC lost " << (info.sent_pixels - info.received_pixels) << " pixels";
}

void KatherineSatellite::starting(std::string_view) {

    const std::lock_guard acquisition_lock {katherine_acq_mutex_};
    const std::lock_guard device_lock {katherine_cmd_mutex_};

    // This needs to be called *before* we start the run thread with acquitions->read
    // otherwise the read function falls through directly with an error
    // This calls katherine_acquisition_begin which:
    // - calls katherine_configure where the hardware is configured via the CTRL UDP socket
    // - changes state to RUNNING
    // - calls katherine_cmd_start_acquisition CTRL UDP to send start cmd to hardware
    acquisition_->begin(katherine_config_, ro_type_);

    // Start Katherine acquisition task:
    acq_future_ = std::async(std::launch::async, [this]() {
        try {
            // This is a blocking call while the acquisition state is RUNNING which receives and
            // decodes data from the measurement data buffer to the pixel buffer, and flushes
            // the pixel buffer to the user code
            const std::lock_guard data_lock {katherine_data_mutex_};
            acquisition_->read();
        } catch(katherine::system_error& e) {
            std::string error_msg {"Katherine error: "};
            error_msg += e.what();
            LOG(CRITICAL) << error_msg;
            throw CommunicationError(error_msg);
        }
    });
    LOG(INFO) << "Spawned acquisition thread";
}

void KatherineSatellite::running(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        if(acq_future_.valid()) {
            const auto state = acq_future_.wait_for(300ms);
            if(state == std::future_status::ready) {
                // Access to rethrow exceptions
                acq_future_.get();
            }
        }
    }
}

void KatherineSatellite::stopping() {

    const std::lock_guard acquisition_lock {katherine_acq_mutex_};

    // Read the current acquisition state:
    if(!acquisition_->aborted()) {
        const std::lock_guard device_lock {katherine_cmd_mutex_};
        // Calls katherine_acq_abort which sends the stop command via CTRL UDP
        acquisition_->abort();
        LOG(DEBUG) << "Aborted acquisition";
    }

    // Wait for acquisition task to finish, i.e. after all current measurement data has been processed and the
    // katherine_acquisition_read call has returned. Then its future is ready, accessing it rethrows any exception that
    // occurred.
    if(acq_future_.valid()) {
        LOG(DEBUG) << "Awaiting future";
        acq_future_.wait();
        LOG(INFO) << "Joined acquisition task";
        acq_future_.get();
    }

    // Add run metadata for end-of-run event:
    setRunMetadataTag("hw_info", get_hw_info());

    // Read status information from acquisition object
    LOG(STATUS) << "Acquisition completed:" << std::endl
                << "state: " << katherine::str_acq_state(acquisition_->state()) << std::endl
                << "received " << acquisition_->completed_frames() << " complete frames";
    LOG_IF(WARNING, acquisition_->dropped_measurement_data() > 0)
        << "Dropped " << acquisition_->dropped_measurement_data() << " measurement data items";
}

std::vector<std::string> KatherineSatellite::get_hw_info() {
    std::lock_guard<std::mutex> lock {katherine_cmd_mutex_};
    auto state = device_->readout_status();
    return {"Type " + to_string(state.hw_type),
            "Revision " + to_string(state.hw_revision),
            "Serial " + to_string(state.hw_serial_number),
            "Firmware " + to_string(state.fw_version)};
}

katherine::dacs KatherineSatellite::parse_dacs_file(std::filesystem::path file_path) const {

    LOG(DEBUG) << "Attempting to read DAC file at " << file_path;
    std::ifstream dacfile(file_path);
    if(!dacfile.good()) {
        throw SatelliteError("Failed to open DAC file at " + file_path.string());
    }

    LOG(INFO) << "Reading DAC file " << file_path;

    katherine::dacs dacs {};
    std::string line;
    while(std::getline(dacfile, line)) {
        // Trim whitespace and line breaks
        line = trim(line);

        // Ignore empty lines or comments
        if(line.empty() || line.front() == '#') {
            continue;
        }

        std::istringstream iss(line);

        int dac_nr, dac_val;
        if(!(iss >> dac_nr >> dac_val)) {
            LOG(DEBUG) << "Read invalid line: " << line;
            break;
        }

        LOG(DEBUG) << "Setting DAC " << dac_nr << " = " << dac_val;

        // Assign dac value
        dacs.array[dac_nr - 1] = dac_val;
    }
    return dacs;
}

katherine::px_config KatherineSatellite::parse_px_config_file(std::filesystem::path file_path) const {

    LOG(INFO) << "Attempting to read pixel configuration file at " << file_path;

    std::ifstream trimfile(file_path);
    if(!trimfile.good()) {
        throw SatelliteError("Failed to open pixel configuration file at " + file_path.string());
    }

    LOG(INFO) << "Reading trimdac file " << file_path;

    // We want to read line by line, pixel by pixel.
    // Katherine expects a array blob of binary data, each pixel has 8bit (char) of data with:
    // 1b: mask, 4b: loc_thl, 1b: testpulse, 2b: reserved
    // We need to end up with 16384 32bit words = 65536 pixel / 4 pixel per word

    // Generate new object and reset memory
    katherine::px_config px_config {};
    memset(&px_config.words, 0, 65536);
    uint32_t* dest = (uint32_t*)px_config.words;

    std::string tline;
    size_t pixels = 0;
    size_t masked = 0;
    size_t tp_enabled = 0;
    while(std::getline(trimfile, tline)) {
        // Trim whitespace and line breaks
        tline = trim(tline);

        // Ignore empty lines or comments
        if(tline.empty() || tline.front() == '#') {
            continue;
        }

        std::istringstream iss(tline);

        int row, col, thr, mask, tp_ena;
        if(!(iss >> col >> row >> thr >> mask >> tp_ena)) {
            LOG(WARNING) << "Read invalid line: " << tline;
            break;
        }

        LOG(TRACE) << "Pixel " << col << ", " << row << ": " << thr << " " << mask << " " << tp_ena;

        // Store to the array - this is harvested from libkatherine's px_config.c:
        auto y = 255 - row;
        uint8_t src = (mask & 0x1) | ((thr & 0xF) << 1) | ((tp_ena & 0x1) << 5); // maybe it's the other way around?
        dest[(64 * col) + (y >> 2)] |= (uint32_t)(src << (8 * (3 - (y % 4))));

        pixels++;
        if(mask) {
            masked++;
        }
        if(tp_ena) {
            tp_enabled++;
        }
    }

    LOG(INFO) << "Read " << pixels << " pixels, " << masked << " masked and " << tp_enabled << " with testpulse enabled";
    return px_config;
}

std::string KatherineSatellite::trim(const std::string& str, const std::string& delims) const {
    auto b = str.find_first_not_of(delims);
    auto e = str.find_last_not_of(delims);
    if(b == std::string::npos || e == std::string::npos) {
        return {};
    }
    return {str, b, e - b + 1};
}