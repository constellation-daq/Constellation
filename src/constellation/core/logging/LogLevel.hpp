/**
 * @file
 * @brief Log levels
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

namespace Constellation {
    // log levels, allows direct casting to spdlog::level::level_enum
    enum class LogLevel : int {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARNING = 3,
        ERROR = 4,
        CRITICAL = 5,
        OFF = 6,
    };
} // namespace Constellation
