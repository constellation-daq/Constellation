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

namespace constellation {
    // Actual Logger implementation
    class Logger {
    public:
        Logger(std::string topic) : topic_(std::move(topic)) {
            // Create logger from global sinks
            spdlog_logger_ = LogSinkManager::getInstance().createLogger(topic_);
        }

        static void setConsoleLogLevel(LogLevel level) {
            // The logger itself forwards all debug messages to sinks by default,
            // console output controlled by the corresponding sink
            LogSinkManager::getInstance().getConsoleSink()->set_level(static_cast<spdlog::level::level_enum>(level));
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

        bool shouldLog(LogLevel level) { return spdlog_logger_->should_log(static_cast<spdlog::level::level_enum>(level)); }

        swap_ostringstream getStream(spdlog::source_loc src_loc, LogLevel level) {
            os_level_ = level;
            source_loc_ = src_loc;
            return {this};
        }

        void log(LogLevel level, std::string_view message) {
            spdlog_logger_->log(static_cast<spdlog::level::level_enum>(level), message);
        }

    private:
        friend swap_ostringstream;
        void flush() {
            // Actually execute logging, needs string copy since this might be async
            spdlog_logger_->log(source_loc_, static_cast<spdlog::level::level_enum>(os_level_), os_.str());

            // Clear the stream by creating a new one
            os_ = std::ostringstream();
        }

        LogLevel os_level_ {LogLevel::OFF};
        std::ostringstream os_;
        spdlog::source_loc source_loc_;

        static constexpr size_t BACKTRACE_MESSAGES {10};
        std::string topic_;
        std::shared_ptr<spdlog::logger> spdlog_logger_;
    };
} // namespace constellation
