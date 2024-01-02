/**
 * @file
 * @brief Logger instance
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/Logger.hpp"

#define GEN_LOGGER_INSTANCE(class_name, logger_topic)                                                                       \
    class class_name : public constellation::Logger {                                                                       \
    public:                                                                                                                 \
        static class_name& getInstance() {                                                                                  \
            static class_name instance {};                                                                                  \
            return instance;                                                                                                \
        }                                                                                                                   \
        class_name(class_name const&) = delete;                                                                             \
        class_name& operator=(class_name const&) = delete;                                                                  \
                                                                                                                            \
    private:                                                                                                                \
        class_name() : constellation::Logger(logger_topic) {                                                                \
            /* debug settings for now*/                                                                                     \
            enableTrace();                                                                                                  \
            setConsoleLogLevel(TRACE);                                                                                      \
        }                                                                                                                   \
    }
