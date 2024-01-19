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

#include <spdlog/async_logger.h>

#include "constellation/core/logging/Level.hpp"

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
         * Log stream that executes logging upon its destruction
         */
        class log_stream final : public std::ostringstream {
        public:
            inline log_stream(Logger& logger, Level level, std::source_location src_loc)
                : logger_(logger), level_(level), src_loc_(src_loc) {}
            inline ~log_stream() final { logger_.log(level_, this->view(), src_loc_); }

            log_stream(log_stream const&) = delete;
            log_stream& operator=(log_stream const&) = delete;
            log_stream(log_stream&&) = delete;
            log_stream& operator=(log_stream&&) = delete;

        private:
            Logger& logger_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
            Level level_;
            std::source_location src_loc_;
        };

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
         * Check if a message should be logged given the currently configured log level
         *
         * @param level Log level to be tested against the logger configuration
         * @return Boolean indicating if the message should be logged
         */
        inline bool shouldLog(Level level) const { return spdlog_logger_->should_log(to_spdlog_level(level)); }

        /**
         * Log a message using a stream
         *
         * @param level Log level of the log message
         * @param src_loc Source code location from which the log message emitted
         * @return a log_stream object
         */
        inline log_stream log(Level level, std::source_location src_loc = std::source_location::current()) {
            return {*this, level, src_loc};
        }

        /**
         * Log a message
         *
         * @param level Level of the log message
         * @param message Log message
         * @param src_loc Source code location from which the log message emitted
         */
        inline void log(Level level,
                        std::string_view message,
                        std::source_location src_loc = std::source_location::current()) {
            spdlog_logger_->log({src_loc.file_name(), static_cast<int>(src_loc.line()), src_loc.function_name()},
                                to_spdlog_level(level),
                                message);
        }

    private:
        static constexpr size_t BACKTRACE_MESSAGES {10};
        std::string topic_;
        std::shared_ptr<spdlog::async_logger> spdlog_logger_;
    };
} // namespace constellation::log
