/**
 * @file
 * @brief Implementation of the Log Sink Manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "LogSinkManager.hpp"

using namespace Constellation;

LogSinkManager& LogSinkManager::getInstance() {
    static LogSinkManager instance {};
    return instance;
}

LogSinkManager::LogSinkManager() {
    // Init thread pool with 1k queue size on 1 thread
    spdlog::init_thread_pool(1000, 1);

    console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink_->set_level(spdlog::level::level_enum::info);

    zmq_sink_ = std::make_shared<zmq_sink_mt>();
    zmq_sink_->set_level(spdlog::level::level_enum::trace);
}
