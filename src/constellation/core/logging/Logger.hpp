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

#include "constellation/core/logging/LogLevel.hpp"
#include "constellation/core/logging/swap_ostringstream.hpp"

namespace constellation {
    // Forward declaration of spdlog implementation class
    class LoggerImplementation;

    // Logger class
    class Logger {
    public:
        Logger(std::string topic);

        void setConsoleLogLevel(LogLevel level);

        // Enable backtrace and sending trace messages over ZeroMQ
        void enableTrace(bool enable = true);

        bool shouldLog(LogLevel level);

        swap_ostringstream getStream(LogLevel level) {
            os_level_ = level;
            return swap_ostringstream(this);
        }

    private:
        friend swap_ostringstream;
        void flush();

        std::shared_ptr<LoggerImplementation> logger_impl_;
        LogLevel os_level_;
        std::ostringstream os_;
    };

} // namespace constellation
