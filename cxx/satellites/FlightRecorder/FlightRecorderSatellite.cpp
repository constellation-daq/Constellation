/**
 * @file
 * @brief Implementation of the Flight Recorder satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "FlightRecorderSatellite.hpp"

#include <format>
#include <string_view>

#include <spdlog/details/log_msg.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "constellation/core/log/log.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/Satellite.hpp"

#include "spdlog/sinks/rotating_file_sink.h"

using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;

FlightRecorderSatellite::FlightRecorderSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), LogListener("LOGRECV", [this](auto&& arg) { add_message(std::forward<decltype(arg)>(arg)); }) {}

void FlightRecorderSatellite::initializing(Configuration& config) {

    const auto path = config.getPath("file_path");

    try {
        if(config.has("rotate_files")) {
            const auto max_files = config.get<size_t>("rotate_files");
            const auto max_size = config.get<size_t>("rotate_filesize", 10) * 1048576; // in bytes
            file_logger_ = spdlog::rotating_logger_mt(getCanonicalName(), path, max_size, max_files);
        } else {
            file_logger_ = spdlog::basic_logger_mt(getCanonicalName(), path);
        }
        spdlog::flush_every(std::chrono::seconds(config.get<size_t>("flush_period", 1)));
    } catch(const spdlog::spdlog_ex& ex) {
        throw SatelliteError(ex.what());
    }

    // Set pattern containing only the timestamp and the message
    file_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");

    // Start the log receiver pool
    startPool();

    // subscribe for all endpoints to global topic:
    const auto global_level = config.get<Level>("global_recording_level", WARNING);
    setGlobalLogLevel(global_level);
}

void FlightRecorderSatellite::starting(std::string_view /*run_identifier*/) {
    // Reset run message count
    msg_logged_run_ = 0;
}

void FlightRecorderSatellite::add_message(CMDP1LogMessage&& msg) {
    if(sink_ == nullptr) {
        return;
    }

    const auto header = msg.getHeader();
    sink_->log(
        header.getTime(),
        {},
        to_spdlog_level(msg.getLogLevel()),
        std::format(
            "[{}] [{}] [{}] {}", header.getSender(), to_string(msg.getLogLevel()), msg.getLogTopic(), msg.getLogMessage()));

    msg_logged_total_++;
    if(getState() == CSCP::State::RUN) {
        msg_logged_run_++;
    }
}
