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

    path_ = validate_file_path(config.getPath("file_path"));
    allow_overwriting_ = config.get<bool>("allow_overwriting");

    try {
        method_ = config.get<LogMethod>("method");
        switch(method_) {
        case LogMethod::FILE: {
            sink_ = spdlog::basic_logger_mt(getCanonicalName(), path_);
            break;
        }
        case LogMethod::ROTATE: {
            const auto max_files = config.get<size_t>("rotate_files");
            const auto max_size = config.get<size_t>("rotate_filesize", 10) * 1048576; // in bytes
            sink_ = spdlog::rotating_logger_mt(getCanonicalName(), path_, max_size, max_files);
            break;
        }
        case LogMethod::DAILY: {
            // FIXME time to be configured
            // const auto time = config.get<size_t>("rotate_files");
            sink_ = spdlog::daily_logger_mt(getCanonicalName(), path_, 14, 55);
            break;
        }
        case LogMethod::RUN: {
            sink_ = spdlog::basic_logger_mt(getCanonicalName(), path_);
            break;
        }
        default: std::unreachable();
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

std::filesystem::path FlightRecorderSatellite::validate_file_path(std::filesystem::path file_path) const {

    // Create all required main directories and possible sub-directories from the filename
    std::filesystem::create_directories(file_path.parent_path());

    // Check if file exists
    if(std::filesystem::is_regular_file(file_path)) {
        if(!allow_overwriting_) {
            throw SatelliteError("Overwriting of existing file " + file_path.string() + " denied");
        }
        LOG(WARNING) << "File " << file_path << " exists and will be overwritten";
        std::filesystem::remove(file_path);
    } else if(std::filesystem::is_directory(file_path)) {
        throw SatelliteError("Requested output file " + file_path.string() + " is a directory");
    }

    // Convert to an absolute path
    return std::filesystem::canonical(file_path);
}

void FlightRecorderSatellite::starting(std::string_view run_identifier) {
    // For method RUN set a new log file:
    if(method_ == LogMethod::RUN) {
        try {
            // Append run identifier to the end of the file name while keeping the extension:
            path_ = validate_file_path(path_.parent_path() / (path_.stem().string() + "_" + std::string(run_identifier) +
                                                              path_.extension().string()));
            sink_ = spdlog::basic_logger_mt(getCanonicalName(), path_);
        } catch(const spdlog::spdlog_ex& ex) {
            throw SatelliteError(ex.what());
        }
    }

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
