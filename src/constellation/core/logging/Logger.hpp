// SPDX-FileCopyrightText: 2022-2023 Stephan Lachnit
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <memory>
#include <string>

#include "Constellation/core/logging/LogLevel.hpp"
#include "Constellation/core/logging/swap_ostringstream.hpp"

namespace Constellation {
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

}
