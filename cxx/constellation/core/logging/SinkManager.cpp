/**
 * @file
 * @brief Implementation of the Log Sink Manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "SinkManager.hpp"

#include <algorithm>
#include <ctime>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <wincon.h>
#endif

#include "constellation/core/logging/CMDPSink.hpp"
#include "constellation/core/logging/Level.hpp"
#include "constellation/core/logging/ProxySink.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::log;
using namespace constellation::utils;

SinkManager::ConstellationLevelFormatter::ConstellationLevelFormatter(bool format_short) : format_short_(format_short) {}

void SinkManager::ConstellationLevelFormatter::format(const spdlog::details::log_msg& msg,
                                                      const std::tm& /*tm*/,
                                                      spdlog::memory_buf_t& dest) {
    auto level_name = to_string(from_spdlog_level(msg.level));
    if(format_short_) {
        // Short format: only first letter
        level_name = level_name.substr(0, 1);
    } else {
        // Long format: pad to 8 characters
        level_name.insert(0, 8 - level_name.size(), ' ');
    }
    dest.append(level_name.data(), level_name.data() + level_name.size());
}

std::unique_ptr<spdlog::custom_flag_formatter> SinkManager::ConstellationLevelFormatter::clone() const {
    return std::make_unique<ConstellationLevelFormatter>(format_short_);
}

void SinkManager::ConstellationTopicFormatter::format(const spdlog::details::log_msg& msg,
                                                      const std::tm& /*tm*/,
                                                      spdlog::memory_buf_t& dest) {
    if(msg.logger_name.size() > 0) {
        auto topic = "[" + to_string(msg.logger_name) + "]";
        dest.append(topic.data(), topic.data() + topic.size());
    }
}

std::unique_ptr<spdlog::custom_flag_formatter> SinkManager::ConstellationTopicFormatter::clone() const {
    return std::make_unique<ConstellationTopicFormatter>();
}

SinkManager& SinkManager::getInstance() {
    static SinkManager instance {};
    return instance;
}

void SinkManager::setGlobalConsoleLevel(Level level) {
    console_sink_->set_level(to_spdlog_level(level));
    // Set new levels for each logger
    for(auto& logger : loggers_) {
        set_cmdp_level(logger);
    }
}

SinkManager::SinkManager() : cmdp_global_level_(OFF) {
    // Init thread pool with 1k queue size on 1 thread
    spdlog::init_thread_pool(1000, 1);

    // Concole sink, log level set via setGlobalConsoleLevel
    console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink_->set_level(to_spdlog_level(TRACE));

    // Set console format, e.g. |2024-01-10 00:16:40.922| CRITICAL [topic] message
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<ConstellationLevelFormatter>('l', false);
    formatter->add_flag<ConstellationLevelFormatter>('L', true);
    formatter->add_flag<ConstellationTopicFormatter>('n');
    formatter->set_pattern("|%Y-%m-%d %H:%M:%S.%e| %^%l%$ %n %v");
    console_sink_->set_formatter(std::move(formatter));

    // Set colors of console sink
#ifdef _WIN32
    console_sink_->set_color(to_spdlog_level(CRITICAL), FOREGROUND_RED | FOREGROUND_INTENSITY);
    console_sink_->set_color(to_spdlog_level(STATUS), FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    console_sink_->set_color(to_spdlog_level(WARNING), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    console_sink_->set_color(to_spdlog_level(INFO), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    console_sink_->set_color(to_spdlog_level(DEBUG), FOREGROUND_GREEN | FOREGROUND_BLUE);
    console_sink_->set_color(to_spdlog_level(TRACE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    console_sink_->set_color(to_spdlog_level(CRITICAL), "\x1B[31;1m"); // Bold red
    console_sink_->set_color(to_spdlog_level(STATUS), "\x1B[32;1m");   // Bold green
    console_sink_->set_color(to_spdlog_level(WARNING), "\x1B[33;1m");  // Bold yellow
    console_sink_->set_color(to_spdlog_level(INFO), "\x1B[36;1m");     // Bold cyan
    console_sink_->set_color(to_spdlog_level(DEBUG), "\x1B[36m");      // Cyan
    console_sink_->set_color(to_spdlog_level(TRACE), "\x1B[90m");      // Grey
#endif

    // Create console logger for CMDP
    cmdp_console_logger_ = std::make_shared<spdlog::async_logger>(
        "CMDP", console_sink_, spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
    cmdp_console_logger_->set_level(to_spdlog_level(TRACE));

    // CMDP sink, log level always TRACE since only accessed via ProxySink
    try {
        cmdp_sink_ = std::make_shared<CMDPSink>(cmdp_console_logger_);
        cmdp_sink_->set_level(to_spdlog_level(TRACE));
    } catch(const zmq::error_t& error) {
        throw ZMQInitError(error.what());
    }

    // Create default logger without topic
    default_logger_ = createLogger("DEFAULT");
}

void SinkManager::enableCMDPSending(std::string sender_name) {
    cmdp_sink_->enableSending(std::move(sender_name));
}

std::shared_ptr<spdlog::async_logger> SinkManager::createLogger(std::string topic, std::optional<Level> console_level) {
    // Create proxy for CMDP sink so that we can set CMDP log level separate from console log level
    auto cmdp_proxy_sink = std::make_shared<ProxySink>(cmdp_sink_);

    // Create proxy for console sink if custom log level was provided
    std::shared_ptr<spdlog::sinks::sink> console_sink {};
    if(console_level.has_value()) {
        console_sink = std::make_shared<ProxySink>(console_sink_);
        console_sink->set_level(to_spdlog_level(console_level.value()));
    } else {
        console_sink = console_sink_;
    }

    auto logger = std::make_shared<spdlog::async_logger>(
        std::move(topic),
        spdlog::sinks_init_list({std::move(console_sink), std::move(cmdp_proxy_sink)}),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest);
    loggers_.push_back(logger);

    // Calculate level of CMDP sink and logger
    set_cmdp_level(logger);

    return logger;
}

void SinkManager::set_cmdp_level(std::shared_ptr<spdlog::async_logger>& logger) {
    // Order of sinks given in createLogger
    auto& console_proxy_sink = logger->sinks().at(0);
    auto& cmdp_proxy_sink = logger->sinks().at(1);

    // Get logger topic in upper-case
    auto logger_topic = transform(logger->name(), ::toupper);

    // First set CMDP proxy level to global CMDP minimum
    Level min_cmdp_proxy_level = cmdp_global_level_;

    // If not default logger
    if(!logger_topic.empty()) {
        // Iterate over topic subscriptions to find minimum level for this logger
        for(auto& [sub_topic, sub_level] : cmdp_sub_topic_levels_) {
            if(logger_topic.starts_with(sub_topic)) {
                // Logger is subscribed => set new minimum level
                min_cmdp_proxy_level = min_level(min_cmdp_proxy_level, sub_level);
            }
        }
    }

    // Set minimum level for CMDP proxy
    cmdp_proxy_sink->set_level(to_spdlog_level(min_cmdp_proxy_level));

    // Calculate level for logger as minimum of both proxy levels
    logger->set_level(to_spdlog_level(min_level(console_proxy_sink->level(), min_cmdp_proxy_level)));
}

void SinkManager::updateCMDPLevels(Level cmdp_global_level, std::map<std::string_view, Level> cmdp_sub_topic_levels) {
    cmdp_global_level_ = cmdp_global_level;
    cmdp_sub_topic_levels_ = std::move(cmdp_sub_topic_levels);
    for(auto& logger : loggers_) {
        set_cmdp_level(logger);
    }
}
