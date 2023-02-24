#pragma once

#include "Constellation/core/logging/Logger.hpp"
#include "Constellation/core/logging/log.hpp"

namespace Constellation {
    class CCLogger : public Logger {
    public:
        static CCLogger& getInstance() {
            static CCLogger instance {};
            return instance;
        }

        CCLogger(CCLogger const&) = delete;
        CCLogger& operator=(CCLogger const&) = delete;

    private:
        CCLogger() : Logger("CC") {
            enableTrace();  // debug settings for now
            setConsoleLogLevel(TRACE);
        }
    };
}

// Logger instance
#define LOGGER Constellation::CCLogger::getInstance()

// If message with level should be logged
#define IFLOG(level) \
    if(LOGGER.shouldLog(level))

// Log message
#define LOG(level) \
    _LOG(LOGGER, level)

// Log message if condition is met
#define LOG_IF(level, condition) \
    _LOG_IF(LOGGER, level, condition)

// Log message at most N times
#define LOG_N(level, count) \
    _LOG_N(LOGGER, level, count)

// Log message at most one time
#define LOG_ONCE(level) \
    _LOG_ONCE(LOGGER, level)
