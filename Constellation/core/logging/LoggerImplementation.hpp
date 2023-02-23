#pragma once

#include "Constellation/core/logging/LogLevel.hpp"

#include <memory>
#include <string>

#include "spdlog/spdlog.h"

#include "Constellation/core/logging/LogSinkManager.hpp"

namespace Constellation {
    // Actual Logger implementation
    class LoggerImplementation {
    public:
        LoggerImplementation(std::string topic)
        :  topic_(std::move(topic)) {
            // Create logger from global sinks
            auto sink_mgr = LogSinkManager::getInstance();
            spdlog_logger_ = sink_mgr->createLogger(topic_);
        }

        void setConsoleLogLevel(LogLevel level) {
            // The logger itself forwards all debug messages to sinks by default,
            // console output controlled by the corresponding sink
            auto sink_mgr = LogSinkManager::getInstance();
            sink_mgr->getConsoleSink()->set_level(static_cast<spdlog::level::level_enum>(level));
        }

        // Enables backtrace and enables TRACE messages over ZeroMQ sink
        void enableTrace(bool enable) {
            if (enable) {
                spdlog_logger_->set_level(spdlog::level::level_enum::trace);
                spdlog_logger_->enable_backtrace(BACKTRACE_MESSAGES);
            }
            else {
                spdlog_logger_->set_level(spdlog::level::level_enum::debug);
                spdlog_logger_->disable_backtrace();
            }
        }

        bool shouldLog(LogLevel level) {
            return spdlog_logger_->should_log(static_cast<spdlog::level::level_enum>(level));
        }

        void log(LogLevel level, std::string message) {
            spdlog_logger_->log(static_cast<spdlog::level::level_enum>(level), message);
        }

    private:
        static constexpr size_t BACKTRACE_MESSAGES {10};
        std::string topic_;
        std::shared_ptr<spdlog::logger> spdlog_logger_;
    };
}
