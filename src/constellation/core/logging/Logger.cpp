/**
 * @file
 * @brief Implementation of the logger
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Logger.hpp"

#include "constellation/core/logging/LoggerImplementation.hpp"

using namespace constellation;

Logger::Logger(std::string topic)
    : logger_impl_(std::make_unique<LoggerImplementation>(std::move(topic))), os_level_(LogLevel::OFF) {}

void Logger::setConsoleLogLevel(LogLevel level) {
    logger_impl_->setConsoleLogLevel(level);
}

void Logger::enableTrace(bool enable) {
    logger_impl_->enableTrace(enable);
}

bool Logger::shouldLog(LogLevel level) {
    return logger_impl_->shouldLog(level);
}

void Logger::flush() {
    // Actually execute logging, needs string copy since this might be async
    logger_impl_->log(os_level_, os_.str());

    // Clear the stream by creating a new one
    os_ = std::ostringstream();
}
