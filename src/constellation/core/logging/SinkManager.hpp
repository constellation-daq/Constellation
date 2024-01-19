/**
 * @file
 * @brief Log Sink Manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <string>

#include <spdlog/async_logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "constellation/core/logging/CMDP1Sink.hpp"

namespace constellation::log {
    // Global manager for sinks
    class SinkManager {
    public:
        static SinkManager& getInstance();

        SinkManager(SinkManager const&) = delete;
        SinkManager& operator=(SinkManager const&) = delete;
        SinkManager(SinkManager&&) = default;
        SinkManager& operator=(SinkManager&&) = default;
        ~SinkManager() = default;

        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> getConsoleSink() { return console_sink_; }

        std::shared_ptr<CMDP1Sink_mt> getCMDPSink() { return cmdp_sink_; }

        std::shared_ptr<spdlog::async_logger> createLogger(std::string logger_name);

    private:
        SinkManager();

        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
        std::shared_ptr<CMDP1Sink_mt> cmdp_sink_;
    };
} // namespace constellation::log
