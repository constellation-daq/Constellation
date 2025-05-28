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

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "constellation/core/log/log.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/Satellite.hpp"

#include "spdlog/sinks/daily_file_sink.h"
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

    // Reset potentially existing sink
    if(sink_ != nullptr) {
        sink_.reset();
    }

    const auto path = config.getPath("file_path");
    const auto method = config.get<LogMethod>("method");

    try {
        if(method == LogMethod::FILE) {
            sink_ = spdlog::basic_logger_mt(getCanonicalName(), path);
        } else if(method == LogMethod::ROTATE) {
            const auto max_files = config.get<size_t>("rotate_files");
            const auto max_size = config.get<size_t>("rotate_filesize", 10) * 1048576; // in bytes
            sink_ = spdlog::rotating_logger_mt(getCanonicalName(), path, max_size, max_files);
        } else if(method == LogMethod::DAILY) {
            // FIXME time to be configured
            // const auto time = config.get<size_t>("rotate_files");
            sink_ = spdlog::daily_logger_mt(getCanonicalName(), path, 14, 55);
        }

        spdlog::flush_every(std::chrono::seconds(config.get<size_t>("flush_period", 1)));
    } catch(const spdlog::spdlog_ex& ex) {
        throw SatelliteError(ex.what());
    }

    // Set pattern containing only the timestamp and the message
    sink_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");

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
