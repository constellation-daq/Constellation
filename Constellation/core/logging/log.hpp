#pragma once

#include "Constellation/core/logging/Logger.hpp"

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
#define _IFLOG(logger, level) \
    if(logger.shouldLog(level))

// Log message
#define _LOG(logger, level) \
    _IFLOG(logger, level) \
        logger.getStream(level)

// Log message if condition is met
#define _LOG_IF(logger, level, condition) \
    _IFLOG(logger, level) \
        if(condition) \
            logger.getStream(level)

// Log message at most N times
#define _LOG_N(logger, level, count) \
    _GENERATE_LOG_VAR(count); \
    _IFLOG(logger, level) \
        if(_GET_LOG_VAR() > 0) \
            logger.getStream(level) \
            << ((--_GET_LOG_VAR() == 0) ? "[further messages suppressed] " : "")

// Log message at most one time
#define _LOG_ONCE(logger, level) \
    _LOG_N(logger, level, 1)
