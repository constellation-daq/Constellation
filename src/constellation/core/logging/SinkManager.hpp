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

#include "constellation/core/logging/zmq_sink.hpp"

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

        std::shared_ptr<zmq_sink_mt> getZeroMQSink() { return zmq_sink_; }

        std::shared_ptr<spdlog::async_logger> createLogger(std::string logger_name) {
            auto logger = std::make_shared<spdlog::async_logger>(std::move(logger_name),
                                                                 spdlog::sinks_init_list({console_sink_, zmq_sink_}),
                                                                 spdlog::thread_pool(),
                                                                 spdlog::async_overflow_policy::overrun_oldest);
            logger->set_level(spdlog::level::level_enum::debug);
            return logger;
        }

    private:
        SinkManager();

        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
        std::shared_ptr<zmq_sink_mt> zmq_sink_;
    };
} // namespace constellation::log
