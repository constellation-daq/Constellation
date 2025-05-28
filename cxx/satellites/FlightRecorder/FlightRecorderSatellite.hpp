/**
 * @file
 * @brief Flight Recorder satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string_view>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/listener/LogListener.hpp"
#include "constellation/satellite/Satellite.hpp"

class FlightRecorderSatellite final : public constellation::satellite::Satellite,
                                      public constellation::listener::LogListener {
private:
    /**
     * @class LogMethod
     * @brief Different logging methods offered by the satellite
     */
    enum class LogMethod {
        FILE,   // Simple log file
        ROTATE, // Multiple log files, rotate logging by file size
        DAILY,  // Create a new log file daily at provided time
        RUN,    // Create a new log file whenever a new run is started
    };

public:
    FlightRecorderSatellite(std::string_view type, std::string_view name);

    void initializing(constellation::config::Configuration& config) final;
    void starting(std::string_view run_identifier) final;

private:
    void add_message(constellation::message::CMDP1LogMessage&& msg);
    std::filesystem::path validate_file_path(std::filesystem::path file_path) const;

private:
    LogMethod method_;
    std::filesystem::path path_;
    bool allow_overwriting_;
    std::shared_ptr<spdlog::logger> sink_;
    std::atomic<std::size_t> msg_logged_total_ {0};
    std::atomic<std::size_t> msg_logged_run_ {0};
};
