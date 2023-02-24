#pragma once

#include "Constellation/core/logging/LogLevel.hpp"

// Forward enum definitions
using enum Constellation::LogLevel;

// Nested concatenation to support __LINE__, see https://stackoverflow.com/a/19666216/17555746
#define _CONCAT_IMPL(x, y) \
    x##y

// Define new token by concatenation with macro support
#define _CONCAT(x, y) \
    _CONCAT_IMPL(x, y)

// Generate a unique log var (contains the line number in the variable name)
#define _GENERATE_LOG_VAR(count) \
    static std::atomic_uint _CONCAT(_LOG_VAR_L, __LINE__) {count}

// Get the unique log variable
#define _GET_LOG_VAR() \
    _CONCAT(_LOG_VAR_L, __LINE__)

// If message with level should be logged
#define IFLOG(level) \
    if(LOGGER.shouldLog(level))

// Log message
#define LOG(level) \
    IFLOG(level) \
        LOGGER.getStream(level)

// Log message if condition is met
#define LOG_IF(level, condition) \
    IFLOG(level) \
        if(condition) \
            LOGGER.getStream(level)

// Log message at most N times
#define LOG_N(level, count) \
    _GENERATE_LOG_VAR(count); \
    IFLOG(level) \
        if(_GET_LOG_VAR() > 0) \
            LOGGER.getStream(level) \
            << ((--_GET_LOG_VAR() == 0) ? "[further messages suppressed] " : "")

// Log message at most one time
#define LOG_ONCE(level) \
    LOG_N(level, 1)
