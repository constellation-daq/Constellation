/**
 * @file
 * @brief Implementation of the Flight Recorder satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "FlightRecorderSatellite.hpp"

#include <string_view>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "constellation/core/log/log.hpp"
#include "constellation/satellite/Satellite.hpp"

#include "spdlog/sinks/rotating_file_sink.h"

using namespace constellation::chirp;
using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::satellite;

FlightRecorderSatellite::FlightRecorderSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), LogListener("LOGRECV", [this](auto&& arg) { add_message(std::forward<decltype(arg)>(arg)); }) {
    // Start the log receiver pool
    startPool();
}

void FlightRecorderSatellite::initializing(Configuration& config) {

    const auto path = config.getPath("file_path");

    try {
        if(config.has("rotate_files")) {
            const auto max_files = config.get<size_t>("rotate_files");
            const auto max_size = config.get<size_t>("rotate_filesize", 10) * 1048576; // in bytes
            file_logger_ = spdlog::rotating_logger_mt("file_logger", path, max_size, max_files);
        } else {
            file_logger_ = spdlog::basic_logger_mt("file_logger", path);
        }
        spdlog::flush_every(std::chrono::seconds(config.get<size_t>("flush_period", 1)));
    } catch(const spdlog::spdlog_ex& ex) {
        throw SatelliteError(ex.what());
    }

    // set custom pattern - add custom flags for sender!
    file_logger_->set_pattern("[%Y-%m-%d %T.%e] [%g/%!] [%l] %v");

    // subscribe for all endpoints to global topic:
    const auto global_level = config.get<Level>("global_recording_level", WARNING);
    setGlobalLogLevel(global_level);
}

void FlightRecorderSatellite::add_message(CMDP1LogMessage&& msg) {
    const auto header = msg.getHeader();
    const auto loc = spdlog::source_loc(std::string(header.getSender()).c_str(), 0, std::string(msg.getLogTopic()).c_str());
    if(file_logger_) {
        file_logger_->log(header.getTime(), loc, to_spdlog_level(msg.getLogLevel()), msg.getLogMessage());
    }
}
