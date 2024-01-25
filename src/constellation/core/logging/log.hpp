/**
 * @file
 * @brief Log macros
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <string_view>

#include "constellation/core/logging/Level.hpp"

using enum constellation::log::Level;                // Forward log level enum
using namespace std::literals::string_view_literals; // NOLINT(google-global-names-in-headers)

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/** Define a new token by concatenation */
#define LOG_CONCAT(x, y) x##y

/** Define new token by concatenation with support for nesting
 *
 * Nesting means that the parameters itself can be a macro. For example,`__LINE__` is a macro and `_VAR_NAME_L##__LINE__`
 * returns `_VAR_NAME_L__LINE__` instead of `_VAR_NAME_LXX`. See also https://stackoverflow.com/a/19666216.
 */
#define LOG_CONCAT_NESTED(x, y) LOG_CONCAT(x, y)

/** Defines a token for a unique log variable named `LOG_VAR_LXX` where `XX` is the line number */
#define LOG_VAR LOG_CONCAT_NESTED(LOG_VAR_L, __LINE__)

/** Returns whether message with certain level be logged */
#define SHOULD_LOG(level) LOGGER.shouldLog(level)

/**
 * Logs a message for a given level
 *
 * The stream expression is only evaluated if logging should take place.
 */
#define LOG(level)                                                                                                          \
    if(LOGGER.shouldLog(level))                                                                                             \
    LOGGER.log(level)

/**
 * Logs a message if a given condition is true
 *
 * The given condition is evaluated after it was determined if logging should take place.
 */
#define LOG_IF(level, condition)                                                                                            \
    if(LOGGER.shouldLog(level) && (condition))                                                                              \
    LOGGER.log(level)

/** Logs a message at most N times */
#define LOG_N(level, count)                                                                                                 \
    static std::atomic_int LOG_VAR {count};                                                                                 \
    if(LOG_VAR > 0 && LOGGER.shouldLog(level))                                                                              \
    LOGGER.log(level) << (--LOG_VAR <= 0 ? "[further messages suppressed] "sv : ""sv)

/** Logs a message at most one time */
#define LOG_ONCE(level) LOG_N(level, 1)

// NOLINTEND(cppcoreguidelines-macro-usage)
