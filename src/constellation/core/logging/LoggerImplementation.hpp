/**
 * @file
 * @brief Logger implementation
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

#include "constellation/core/logging/LogLevel.hpp"
#include "constellation/core/logging/LogSinkManager.hpp"

namespace Constellation {
    // Actual Logger implementation
    class LoggerImplementation {
    public:
        LoggerImplementation(std::string topic) : topic_(std::move(topic)) {
            // Create logger from global sinks
            spdlog_logger_ = LogSinkManager::getInstance().createLogger(topic_);
        }

        void setConsoleLogLevel(LogLevel level) {
            // The logger itself forwards all debug messages to sinks by default,
            // console output controlled by the corresponding sink
            LogSinkManager::getInstance().getConsoleSink()->set_level(static_cast<spdlog::level::level_enum>(level));
        }

        // Enables backtrace and enables TRACE messages over ZeroMQ sink
        void enableTrace(bool enable) {
            if(enable) {
                spdlog_logger_->set_level(spdlog::level::level_enum::trace);
                spdlog_logger_->enable_backtrace(BACKTRACE_MESSAGES);
            } else {
                spdlog_logger_->set_level(spdlog::level::level_enum::debug);
                spdlog_logger_->disable_backtrace();
            }
        }

        bool shouldLog(LogLevel level) { return spdlog_logger_->should_log(static_cast<spdlog::level::level_enum>(level)); }

        void log(LogLevel level, std::string message) {
            spdlog_logger_->log(static_cast<spdlog::level::level_enum>(level), message);
        }

    private:
        static constexpr size_t BACKTRACE_MESSAGES {10};
        std::string topic_;
        std::shared_ptr<spdlog::logger> spdlog_logger_;
    };
} // namespace Constellation
