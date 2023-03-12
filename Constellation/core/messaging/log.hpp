// SPDX-FileCopyrightText: 2022-2023 Stephan Lachnit
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include "Constellation/core/logging/LoggerInstance.hpp"

namespace Constellation {
    // Create definition of MessagingLoggerLogger class
    GEN_LOGGER_INSTANCE(MessagingLogger, "Messaging");
}

// Use ControlCenter::Logger instance in LOG macros
#define LOGGER Constellation::MessagingLogger::getInstance()
