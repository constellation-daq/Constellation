/**
 * @file
 * @brief Logger
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <source_location>
#include <string>

#include <spdlog/logger.h>

#include "constellation/core/logging/Level.hpp"
#include "constellation/core/logging/swap_ostringstream.hpp"

namespace constellation::log {
    /**
     * \class Logger
     * \brief Wrapper class for spdlog logger
     *
     * \details This class implements a wrapper around the spdlog logger and provides additional features such as the
     * the possibility to log using ostringstreams (with << syntax rather than enclosing the log message in parentheses)
     */
    class Logger {
    public:
        /**
         * \brief Constructor
         * \details This creates a new Logger object and registers the spdlog logger with the static SinkManager
         *
         * \param topic Name (topic) of the logger
         */
        Logger(std::string topic);

        /**
         * \brief Set logging level for console
         *
         * \param level Log level for console
         */
        static void setConsoleLogLevel(Level level);

        /**
         * \brief Enable or disable TRACE-level messages and backtrace
         * \param enable Boolean to enable TRACE messages
         */
        void enableTrace(bool enable = true);

        /**
         * \brief Wrapper around spdlog's should_log to check if a message should be logged given the currently configured
         * log level of the logger
         *
         * \param level Log level to be tested against the logger configuration
         * \return Boolean indicating if the message should be logged
         */
        bool shouldLog(Level level) const;

        /**
         * \brief Helper method returning the output stream of the logger
         *
         * \param src_loc Source code location from which the log message emitted
         * \param level Log level of the emitted log message
         *
         * \return swap_ostringstream object
         */
        swap_ostringstream log(Level level, std::source_location loc = std::source_location::current());

        /**
         * \brief Wrapper around the spdlog logger's log method
         *
         * \param level Level of the log message
         * \param message Log message
         */
        void log(Level level, std::string_view message);

    private:
        friend swap_ostringstream;

        /**
         * \brief Helper method to flush the swap_ostringstream content to the spdlog logger
         */
        void flush();

        Level os_level_ {Level::OFF};
        std::ostringstream os_;
        std::source_location source_loc_;

        static constexpr size_t BACKTRACE_MESSAGES {10};
        std::string topic_;
        std::shared_ptr<spdlog::logger> spdlog_logger_;
    };
} // namespace constellation::log
