/**
 * @file
 * @brief Implementation of Heartbeat sender
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "HeartbeatSend.hpp"

#include <chrono>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/ports.hpp"

using namespace constellation;
using namespace constellation::heartbeat;
using namespace constellation::message;
using namespace constellation::utils;

HeartbeatSend::HeartbeatSend(std::string sender, std::chrono::milliseconds interval)
    : pub_(context_, zmq::socket_type::pub), port_(bind_ephemeral_port(pub_)), sender_(std::move(sender)),
      interval_(interval) {

    // Announce service via CHIRP
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->registerService(chirp::HEARTBEAT, port_);
    }

    sender_thread_ = std::jthread(std::bind_front(&HeartbeatSend::loop, this));
}

HeartbeatSend::~HeartbeatSend() {

    // Send CHIRP depart message
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->unregisterService(chirp::HEARTBEAT, port_);
    }

    // Stop sender thread
    sender_thread_.request_stop();
    cv_.notify_one();
    if(sender_thread_.joinable()) {
        sender_thread_.join();
    }
}

void HeartbeatSend::updateState(State state) {
    // Update state and wake up sending thread
    state_ = state;
    cv_.notify_one();
}

void HeartbeatSend::loop(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        std::unique_lock<std::mutex> lock {mutex_};

        // Get currently configured interval
        const auto interval = interval_.load();

        // Publish CHP message with current state
        CHP1Message(sender_, state_, interval).assemble().send(pub_);

        // Wait until either the interval before sending the next regular heartbeat has passed - or the state changes
        // The state is captured in the lambda as copy upon invocation such that it represents the state when entering the
        // waiting state
        cv_.wait_until(lock, std::chrono::system_clock::now() + interval / 2, [&, state = state_.load()]() {
            return stop_token.stop_requested() || state != state_;
        });
        lock.unlock();
    }
}
