/**
 * @file
 * @brief Log levels
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

// Overwrite default log level names
#define SPDLOG_LEVEL_NAMES                                                                                                  \
    { "TRACE", "DEBUG", "INFO", "WARNING", "STATUS", "CRITICAL", "OFF" }
#define SPDLOG_SHORT_LEVEL_NAMES                                                                                            \
    { "T", "D", "I", "W", "S", "C", "O" }

// Disable default logger
#define SPDLOG_DISABLE_DEFAULT_LOGGER

// Specify all required spdlog headers here, do not include spdlog headers elsewhere
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include "spdlog/details/null_mutex.h"
#include "spdlog/sinks/base_sink.h"

namespace constellation {
    // log levels, allows direct casting to spdlog::level::level_enum
    enum class Level : int {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARNING = 3,
        STATUS = 4,
        CRITICAL = 5,
        OFF = 6,
    };

    constexpr spdlog::level::level_enum to_spdlog_level(Level level) {
        return static_cast<spdlog::level::level_enum>(level);
    }
} // namespace constellation
