/**
 * @file
 * @brief Implementation of the CHP manager
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Manager.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iterator>
#include <utility>

#include "constellation/core/logging/log.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std23.hpp"

using namespace constellation::heartbeat;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;

Manager::Manager(std::string_view sender) : receiver_(), sender_(sender, 1000ms), logger_("CHP") {}

Manager::~Manager() {
    receiver_thread_.request_stop();
    sender_thread_.request_stop();

    if(receiver_thread_.joinable()) {
        receiver_thread_.join();
    }
    if(sender_thread_.joinable()) {
        sender_thread_.join();
    }
}

std::function<void(State)> Manager::getCallback() {
    return std::bind(&HeartbeatSend::updateState, &sender_, std::placeholders::_1);
}

void Manager::start() {
    // jthread immediately starts on construction
    receiver_thread_ = std::jthread(std::bind_front(&HeartbeatRecv::loop, &receiver_));
    sender_thread_ = std::jthread(std::bind_front(&HeartbeatSend::loop, &sender_));
}
