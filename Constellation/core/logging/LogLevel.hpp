#pragma once

namespace Constellation {
    // log levels, allows direct casting to spdlog::level::level_enum
    enum class LogLevel : int {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        CRITICAL = 5,
        OFF = 6,
    };
}
