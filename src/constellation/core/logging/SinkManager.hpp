/**
 * @file
 * @brief Log Sink Manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <string>

#include <spdlog/async_logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "constellation/core/config.hpp"
#include "constellation/core/logging/CMDP1Sink.hpp"

namespace constellation::log {
    /**
     * Global sink manager
     *
     * This class manager the console and CMDP1 sinks and can creates new spdlog loggers.
     */
    class SinkManager {
    public:
        CNSTLN_API static SinkManager& getInstance();

        SinkManager(SinkManager const&) = delete;
        SinkManager& operator=(SinkManager const&) = delete;
        SinkManager(SinkManager&&) = default;
        SinkManager& operator=(SinkManager&&) = default;
        ~SinkManager() = default;

        /**
         * Set the global (default) console log level
         *
         * @param level Log level for console output
         */
        CNSTLN_API void setGlobalConsoleLevel(Level level) const;

        /**
         * Create a new asynchronous spglog logger
         *
         * @param topic Topic of the new logger
         * @return Shared pointer to the new logger
         */
        CNSTLN_API std::shared_ptr<spdlog::async_logger> createLogger(std::string topic);

    private:
        SinkManager();

        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
        std::shared_ptr<CMDP1Sink_mt> cmdp_sink_;
    };
} // namespace constellation::log
