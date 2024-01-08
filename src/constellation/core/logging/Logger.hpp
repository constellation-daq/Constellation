/**
 * @file
 * @brief Logger
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <string>

#include "constellation/core/logging/level.h"
#include "constellation/core/logging/SinkManager.hpp"
#include "constellation/core/logging/swap_ostringstream.hpp"

namespace constellation::log {
    // Actual Logger implementation
    class Logger {
    public:
        Logger(std::string topic) : topic_(std::move(topic)) {
            // Create logger from global sinks
            spdlog_logger_ = SinkManager::getInstance().createLogger(topic_);
        }

        static void setConsoleLogLevel(Level level) {
            // The logger itself forwards all debug messages to sinks by default,
            // console output controlled by the corresponding sink
            SinkManager::getInstance().getConsoleSink()->set_level(to_spdlog_level(level));
        }

        // Enables backtrace and enables TRACE messages over ZeroMQ sink
        void enableTrace(bool enable = true) {
            if(enable) {
                spdlog_logger_->set_level(spdlog::level::level_enum::trace);
                spdlog_logger_->enable_backtrace(BACKTRACE_MESSAGES);
            } else {
                spdlog_logger_->set_level(spdlog::level::level_enum::debug);
                spdlog_logger_->disable_backtrace();
            }
        }

        bool shouldLog(Level level) { return spdlog_logger_->should_log(to_spdlog_level(level)); }

        swap_ostringstream getStream(spdlog::source_loc src_loc, Level level) {
            os_level_ = level;
            source_loc_ = src_loc;
            return {this};
        }

        void log(Level level, std::string_view message) { spdlog_logger_->log(to_spdlog_level(level), message); }

    private:
        friend swap_ostringstream;
        void flush() {
            // Actually execute logging, needs string copy since this might be async
            spdlog_logger_->log(source_loc_, to_spdlog_level(os_level_), os_.str());

            // Clear the stream by creating a new one
            os_ = std::ostringstream();
        }

        Level os_level_ {Level::OFF};
        std::ostringstream os_;
        spdlog::source_loc source_loc_;

        static constexpr size_t BACKTRACE_MESSAGES {10};
        std::string topic_;
        std::shared_ptr<spdlog::logger> spdlog_logger_;
    };
} // namespace constellation::log
