/**
 * @file
 * @brief Log levels
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#define SPDLOG_LEVEL_NAMES { "TRACE", "DEBUG", "INFO", "WARNING", "STATUS", "CRITICAL", "OFF" }
#define SPDLOG_SHORT_LEVEL_NAMES { "T", "D", "I", "W", "S", "C", "O" }

namespace Constellation {
    // log levels, allows direct casting to spdlog::level::level_enum
    enum class LogLevel : int {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARNING = 3,
        STATUS = 4,
        CRITICAL = 5,
        OFF = 6,
    };
} // namespace Constellation
