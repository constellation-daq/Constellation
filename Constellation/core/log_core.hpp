#pragma once

#include "Constellation/core/logging/Logger.hpp"
#include "Constellation/core/logging/log.hpp"

namespace Constellation {
    class CoreLogger : public Logger {
    public:
        static CoreLogger& getInstance() {
            static CoreLogger instance {};
            return instance;
        }

        CoreLogger(CoreLogger const&) = delete;
        CoreLogger& operator=(CoreLogger const&) = delete;

    private:
        CoreLogger() : Logger("Core") {
            enableTrace();  // debug settings for now
            setConsoleLogLevel(TRACE);
        }
    };
}

// Log message
#define LOG(level) \
    _LOG(Constellation::CoreLogger::getInstance(), level)

// Log message if condition is met
#define LOG_IF(level, condition) \
    _LOG_IF(Constellation::CoreLogger::getInstance(), level, condition)

// Log message at most N times
#define LOG_N(level, count) \
    _LOG_N(Constellation::CoreLogger::getInstance(), level, count)

// Log message at most one time
#define LOG_ONCE(level) \
    _LOG_ONCE(Constellation::CoreLogger::getInstance(), level)
