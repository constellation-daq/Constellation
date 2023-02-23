#pragma once

#include <memory>
#include <string>

#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "Constellation/core/logging/zmq_sink.hpp"

namespace Constellation {
    // Global manager for sinks
    class LogSinkManager {
    public:
        static LogSinkManager& getInstance();

        LogSinkManager(LogSinkManager const&) = delete;
        LogSinkManager& operator=(LogSinkManager const&) = delete;

        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> getConsoleSink() {
            return console_sink_;
        }

        std::shared_ptr<zmq_sink_mt> getZeroMQSink() {
            return zmq_sink_;
        }

        std::shared_ptr<spdlog::async_logger> createLogger(std::string logger_name) {
            auto logger = std::make_shared<spdlog::async_logger>(std::move(logger_name), spdlog::sinks_init_list({console_sink_, zmq_sink_}), spdlog::thread_pool());
            logger->set_level(spdlog::level::level_enum::debug);
            return logger;
        }

    private:
        LogSinkManager();

        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
        std::shared_ptr<zmq_sink_mt> zmq_sink_;
    };
}
