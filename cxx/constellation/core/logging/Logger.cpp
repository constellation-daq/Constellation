/**
 * @file
 * @brief Implementation of Logger
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Logger.hpp"

#include <chrono>
#include <thread>

#include "constellation/core/logging/SinkManager.hpp"

using namespace constellation::log;
using namespace std::literals::chrono_literals;

Logger::Logger(std::string topic, std::optional<Level> console_level)
    : spdlog_logger_(SinkManager::getInstance().createLogger(std::move(topic), console_level)) {}

Logger& Logger::getDefault() {
    static Logger instance {SinkManager::getInstance().getDefaultLogger()};
    return instance;
}

Logger::~Logger() {
    flush();
}

void Logger::flush() {
    for(auto& sink : spdlog_logger_->sinks()) {
        sink->flush();
    }
    // Wait a bit to ensure console output is actually flushed
    std::this_thread::sleep_for(1ms);
}
