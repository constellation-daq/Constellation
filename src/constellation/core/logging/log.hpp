/**
 * @file
 * @brief Log macros
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "constellation/core/logging/LogLevel.hpp"

// Forward enum definitions
using enum constellation::LogLevel;

// Nested concatenation to support __LINE__, see https://stackoverflow.com/a/19666216/17555746
#define CONCAT_IMPL(x, y) x##y

// Define new token by concatenation with macro support
#define CONCAT(x, y) CONCAT_IMPL(x, y)

// Generate a unique log var (contains the line number in the variable name)
#define GENERATE_LOG_VAR(count)                                                                                             \
    static std::atomic_uint CONCAT(_LOG_VAR_L, __LINE__) {                                                                  \
        count                                                                                                               \
    }

// Get the unique log variable
#define GET_LOG_VAR() CONCAT(_LOG_VAR_L, __LINE__)

// If message with level should be logged
#define IFLOG(level) if(LOGGER.shouldLog(level))

// Log message
#define LOG(level)                                                                                                          \
    IFLOG(level)                                                                                                            \
    LOGGER.getStream(spdlog::source_loc {__FILE_NAME__, __LINE__, SPDLOG_FUNCTION}, level)

// Log message if condition is met
#define LOG_IF(level, condition)                                                                                            \
    IFLOG(level)                                                                                                            \
    if(condition)                                                                                                           \
    LOGGER.getStream(spdlog::source_loc {__FILE_NAME__, __LINE__, SPDLOG_FUNCTION}, level)

// Log message at most N times
#define LOG_N(level, count)                                                                                                 \
    GENERATE_LOG_VAR(count);                                                                                                \
    IFLOG(level)                                                                                                            \
    if(GET_LOG_VAR() > 0)                                                                                                   \
    LOGGER.getStream(spdlog::source_loc {__FILE_NAME__, __LINE__, SPDLOG_FUNCTION}, level)                                  \
        << ((--GET_LOG_VAR() == 0) ? "[further messages suppressed] " : "")

// Log message at most one time
#define LOG_ONCE(level) LOG_N(level, 1)
