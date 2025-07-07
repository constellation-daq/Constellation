/**
 * @file
 * @brief Flight Recorder satellite
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

#include <spdlog/logger.h>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/listener/LogListener.hpp"
#include "constellation/satellite/Satellite.hpp"

/**
 * @class FlightRecorderSatellite
 * @brief Satellite to receive and record log messages from the constellation
 */
class FlightRecorderSatellite final : public constellation::satellite::Satellite,
                                      public constellation::listener::LogListener {
private:
    /**
     * @class LogMethod
     * @brief Different logging methods offered by the satellite
     */
    enum class LogMethod : std::uint8_t {
        FILE,   // Simple log file
        ROTATE, // Multiple log files, rotate logging by file size
        DAILY,  // Create a new log file daily at provided time
        RUN,    // Create a new log file whenever a new run is started
    };

public:
    FlightRecorderSatellite(std::string_view type, std::string_view name);

    void initializing(constellation::config::Configuration& config) final;
    void landing() final;
    void starting(std::string_view run_identifier) final;
    void stopping() final;
    void interrupting(constellation::protocol::CSCP::State previous_state, std::string_view reason) final;
    void failure(constellation::protocol::CSCP::State previous_state, std::string_view reason) final;

private:
    /**
     * @brief Callback method for receiving log messages and sending them to the log sink
     *
     * @param msg Received CMDP log message
     */
    void log_message(constellation::message::CMDP1LogMessage&& msg);

    /**
     * @brief Helper function to check a file path for validity
     * @details This tests whether the file exists and either deletes it or throws an exception, depending on whether the
     *          "allow_overwrite" flag is set or not. If not a file, it checks if it is an existing directory. The method
     *          creates all parent folders, attempts to open the file and converts the path to a canonical file path
     *
     * @param file_path Input file path to be validated
     * @return Canonicalized file path
     */
    std::filesystem::path validate_file_path(const std::filesystem::path& file_path) const;

private:
    LogMethod method_ {LogMethod::FILE};
    std::filesystem::path path_;
    bool allow_overwriting_ {false};
    std::shared_ptr<spdlog::logger> sink_;
    std::atomic<std::size_t> msg_logged_total_ {0};
    std::atomic<std::size_t> msg_logged_warning_ {0};
    std::atomic<std::size_t> msg_logged_run_ {0};
};
