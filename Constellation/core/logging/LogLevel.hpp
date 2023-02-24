#pragma once

namespace Constellation {
    // log levels, allows direct casting to spdlog::level::level_enum
    enum class LogLevel : int {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARNING = 3,
        ERROR = 4,
	STATUS = 5,
        CRITICAL = 6,
        OFF = 7,
    };
}
