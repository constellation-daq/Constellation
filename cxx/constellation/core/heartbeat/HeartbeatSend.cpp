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
#include <cstdint>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/networking/exceptions.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/protocol/CHP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/thread.hpp"

using namespace constellation::heartbeat;
using namespace constellation::message;
using namespace constellation::networking;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::chrono_literals;

HeartbeatSend::HeartbeatSend(std::string sender,
                             std::function<CSCP::State()> state_callback,
                             std::chrono::milliseconds interval)
    : pub_socket_(*global_zmq_context(), zmq::socket_type::xpub), port_(bind_ephemeral_port(pub_socket_)),
      sender_(std::move(sender)), state_callback_(std::move(state_callback)), default_interval_(interval),
      interval_(CHP::MinimumInterval), default_flags_(CHP::flags_from_role(CHP::Role::DYNAMIC)),
      flags_(default_flags_.load()) {

    // Enable XPub verbosity to receive all subscription and unsubscription messages:
    pub_socket_.set(zmq::sockopt::xpub_verboser, true);

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
    flags_ = flags_.load() | CHP::MessageFlags::IS_EXTRASYSTOLE;
    cv_.notify_one();
}

void HeartbeatSend::loop(const std::stop_token& stop_token) {
    // Notify condition variable when stop is requested
    const std::stop_callback stop_callback {stop_token, [&]() { cv_.notify_all(); }};

    while(!stop_token.stop_requested()) {
        std::unique_lock<std::mutex> lock {mutex_};
        // Wait until condition variable is notified or timeout of interval is reached, send 20% earlier than promised
        cv_.wait_for(lock, interval_.load() * 0.8);

        try {
            // Handle subscriptions to update subscriber count
            bool received = true;
            while(received) {
                zmq::multipart_t recv_msg {};
                received = recv_msg.recv(pub_socket_, static_cast<int>(zmq::send_flags::dontwait));

                // Break if timed out or wrong number of frames received
                if(!received || recv_msg.size() != 1) {
                    break;
                }

                // First byte \x01 is subscription, \0x00 is unsubscription
                const auto subscribe = static_cast<bool>(*recv_msg.front().data<uint8_t>());
                subscribers_ += (subscribe ? 1 : -1);
            }

            // Update the interval based on the amount of subscribers:
            interval_ = CHP::calculate_interval(subscribers_, default_interval_);

            // Publish CHP message with current state and the updated interval
            CHP1Message(sender_, state_callback_(), interval_.load(), flags_.load(), status_).assemble().send(pub_socket_);
            // Reset status and flags to default flags
            status_.reset();
            flags_.store(default_flags_.load());
        } catch(const zmq::error_t& e) {
            throw NetworkError(e.what());
        }
    }
}
