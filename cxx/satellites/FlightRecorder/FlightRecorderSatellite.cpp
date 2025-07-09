/**
 * @file
 * @brief Implementation of the Flight Recorder satellite
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "FlightRecorderSatellite.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <utility>

#ifdef __cpp_lib_format
#include <format>
#else
#include <sstream>
#endif

#if __cpp_lib_chrono < 201907L
#include <ctime>
#endif

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/listener/LogListener.hpp"
#include "constellation/satellite/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::metrics;
using namespace constellation::protocol::CSCP;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;

FlightRecorderSatellite::FlightRecorderSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), LogListener("MNTR", [this](auto&& arg) { log_message(std::forward<decltype(arg)>(arg)); }) {

    register_timed_metric("MSG_TOTAL",
                          "",
                          MetricType::LAST_VALUE,
                          "Total number messages received and logged since satellite startup",
                          3s,
                          [this]() { return msg_logged_total_.load(); });
    register_timed_metric("MSG_WARN",
                          "",
                          MetricType::LAST_VALUE,
                          "Number of warning messages received and logged since satellite startup",
                          3s,
                          [this]() { return msg_logged_warning_.load(); });
    register_timed_metric("MSG_RUN",
                          "",
                          MetricType::LAST_VALUE,
                          "Total number messages received and logged since the last run start",
                          3s,
                          [this]() { return msg_logged_run_.load(); });

    register_command(
        "flush", "Flush log sink", {State::INIT, State::ORBIT, State::RUN, State::SAFE}, [this]() { sink_->flush(); });
}

void FlightRecorderSatellite::initializing(Configuration& config) {
    // Stop pool in case it was already started
    stopPool();

    // Reset potentially existing sink
    sink_.reset();

    method_ = config.get<LogMethod>("method");
    allow_overwriting_ = config.get<bool>("allow_overwriting", false);
    path_ = validate_file_path(config.getPath("file_path"));

    try {
        switch(method_) {
        case LogMethod::FILE: {
            sink_ = spdlog::basic_logger_mt(getCanonicalName(), path_.string());
            break;
        }
        case LogMethod::ROTATE: {
            const auto max_files = config.get<std::size_t>("rotate_max_files", 10);
            const auto max_size = config.get<std::size_t>("rotate_filesize", 100) * 1048576; // in bytes
            sink_ = spdlog::rotating_logger_mt(getCanonicalName(), path_.string(), max_size, max_files);
            break;
        }
        case LogMethod::DAILY: {
            // Get timestamp and convert to local time
            const auto daily_switchting_time = config.get<std::chrono::system_clock::time_point>("daily_switching_time");

#if __cpp_lib_chrono >= 201907L
            const auto local_time = std::chrono::current_zone()->to_local(daily_switchting_time);
            const std::chrono::hh_mm_ss t {local_time - std::chrono::floor<std::chrono::days>(local_time)};
#else
            const auto time_t = std::chrono::system_clock::to_time_t(daily_switchting_time);
            std::tm tm {};
            localtime_r(&time_t, &tm); // there is no thread-safe std::localtime
            const std::chrono::hh_mm_ss t {std::chrono::hours {tm.tm_hour} + std::chrono::minutes {tm.tm_min}};
#endif

            LOG(INFO) << "Daily log file change will be triggered at " << t.hours().count() << ":" << t.minutes().count();
            sink_ = spdlog::daily_logger_mt(getCanonicalName(),
                                            path_.string(),
                                            static_cast<int>(t.hours().count()),
                                            static_cast<int>(t.minutes().count()));
            break;
        }
        case LogMethod::RUN: {
            sink_ = spdlog::basic_logger_mt(getCanonicalName(), path_.string());
            break;
        }
        default: std::unreachable();
        }

        spdlog::flush_every(std::chrono::seconds(config.get<std::size_t>("flush_period", 10)));
    } catch(const spdlog::spdlog_ex& ex) {
        throw SatelliteError(ex.what());
    }
    LOG(INFO) << "Start logging to log file " << path_;

    // Set pattern containing only the timestamp and the message
    sink_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");

    // Start the log receiver pool
    startPool();

    // Subscribe for all endpoints to global topic
    const auto global_level = config.get<Level>("global_recording_level", WARNING);
    setGlobalLogLevel(global_level);
}

std::filesystem::path FlightRecorderSatellite::validate_file_path(const std::filesystem::path& file_path) const {

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

    // Open the file to check if it can be accessed
    const auto file_stream = std::ofstream(file_path);
    if(!file_stream.good()) {
        throw SatelliteError("File " + file_path.string() + " not accessible");
    }

    // Convert to an absolute path
    return std::filesystem::canonical(file_path);
}

void FlightRecorderSatellite::landing() {
    // Force a flush when landing
    sink_->flush();
}

void FlightRecorderSatellite::starting(std::string_view run_identifier) {
    // For method RUN set a new log file
    if(method_ == LogMethod::RUN) {
        try {
            // Append run identifier to the end of the file name while keeping the extension
            const auto path =
                validate_file_path(path_.parent_path() /
                                   (path_.stem().string() + "_" + std::string(run_identifier) + path_.extension().string()));
            sink_ = spdlog::basic_logger_mt(getCanonicalName(), path.string());
            sink_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
        } catch(const spdlog::spdlog_ex& ex) {
            throw SatelliteError(ex.what());
        }
        LOG(INFO) << "Switched to new log file " << path_;
    }

    // Reset run message count
    msg_logged_run_ = 0;
}

void FlightRecorderSatellite::stopping() {
    // Force a flush at run stop
    sink_->flush();
}

void FlightRecorderSatellite::interrupting(State /*previous_state*/, std::string_view /*reason*/) {
    // Force a flush at interruption
    sink_->flush();
}

void FlightRecorderSatellite::failure(State /*previous_state*/, std::string_view /*reason*/) {
    try {
        if(sink_ != nullptr) {
            sink_->flush();
        }
    } catch(...) {
        LOG(CRITICAL) << "Failed to flush logs";
    }
    stopPool();
}

// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
void FlightRecorderSatellite::log_message(CMDP1LogMessage&& msg) {
    const auto& header = msg.getHeader();

#ifdef __cpp_lib_format
    const auto log_msg = std::format(
        "[{}] [{}] [{}] {}", header.getSender(), to_string(msg.getLogLevel()), msg.getLogTopic(), msg.getLogMessage());
#else
    std::ostringstream oss;
    oss << "[" << header.getSender() << "] [" << to_string(msg.getLogLevel()) << "] [" << msg.getLogTopic() << "] "
        << msg.getLogMessage();
    const auto log_msg = oss.str();
#endif

    // Sink the message
    sink_->log(header.getTime(), {}, to_spdlog_level(msg.getLogLevel()), log_msg);

    // Update statistics
    msg_logged_total_++;
    if(getState() == State::RUN) {
        msg_logged_run_++;
    }
    if(msg.getLogLevel() == Level::WARNING) {
        msg_logged_warning_++;
    }
}
