#pragma once

#include "Constellation/core/logging/LoggerInstance.hpp"

namespace Constellation {
    namespace Core {
        // Create definition of Core::Logger class
        GEN_LOGGER_INSTANCE(Logger, "Core");
    }
}

// Use Core::Logger instance in LOG macros
#define LOGGER Constellation::Core::Logger::getInstance()
