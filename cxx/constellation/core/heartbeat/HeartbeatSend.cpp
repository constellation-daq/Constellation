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
#include <string_view>
#include <thread>
#include <utility>

#include <zmq.hpp>

#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/networking/exceptions.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/thread.hpp"

using namespace constellation::heartbeat;
using namespace constellation::message;
using namespace constellation::networking;
using namespace constellation::protocol;
using namespace constellation::utils;

HeartbeatSend::HeartbeatSend(std::string sender,
                             std::function<CSCP::State()> state_callback,
                             std::chrono::milliseconds interval)
    : pub_socket_(*global_zmq_context(), zmq::socket_type::pub), port_(bind_ephemeral_port(pub_socket_)),
      sender_(std::move(sender)), state_callback_(std::move(state_callback)), interval_(interval) {

    // Announce service via CHIRP
    auto* chirp_manager = ManagerLocator::getCHIRPManager();
    if(chirp_manager != nullptr) {
        chirp_manager->registerService(CHIRP::HEARTBEAT, port_);
    }

    sender_thread_ = std::jthread(std::bind_front(&HeartbeatSend::loop, this));
    set_thread_name(sender_thread_, "HeartbeatSend");
}

HeartbeatSend::~HeartbeatSend() {
    terminate();
}

void HeartbeatSend::terminate() {
    // Send CHIRP depart message
    auto* chirp_manager = ManagerLocator::getCHIRPManager();
    if(chirp_manager != nullptr) {
        chirp_manager->unregisterService(CHIRP::HEARTBEAT, port_);
    }
    // Stop sender thread
    sender_thread_.request_stop();
    if(sender_thread_.joinable()) {
        sender_thread_.join();
    }
}

void HeartbeatSend::sendExtrasystole(std::string_view status) {
    if(!status.empty()) {
        const std::lock_guard lock {mutex_};
        status_ = status;
    }
    cv_.notify_one();
}

void HeartbeatSend::loop(const std::stop_token& stop_token) {
    // Notify condition variable when stop is requested
    const std::stop_callback stop_callback {stop_token, [&]() { cv_.notify_all(); }};

    while(!stop_token.stop_requested()) {
        std::unique_lock<std::mutex> lock {mutex_};
        // Wait until condition variable is notified or timeout is reached
        cv_.wait_for(lock, interval_.load() / 2);

        try {
            // Publish CHP message with current state
            CHP1Message(sender_, state_callback_(), interval_.load(), status_).assemble().send(pub_socket_);
            status_.reset();
        } catch(const zmq::error_t& e) {
            throw NetworkError(e.what());
        }
    }
}
