#pragma once

#include "Constellation/core/logging/LoggerInstance.hpp"

namespace Constellation {
    namespace ControlCenter {
        // Create definition of ControlCenter::Logger class
        GEN_LOGGER_INSTANCE(Logger, "ControlCenter");
    }
}

// Use ControlCenter::Logger instance in LOG macros
#define LOGGER Constellation::ControlCenter::Logger::getInstance()
