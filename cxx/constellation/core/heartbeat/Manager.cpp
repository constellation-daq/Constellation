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

Manager::Manager(std::string_view sender)
    : receiver_(std::bind(&Manager::process_heartbeat, this, std::placeholders::_1)), sender_(sender, 1000ms),
      logger_("CHP") {}

Manager::~Manager() {
    watchdog_thread_.request_stop();
    receiver_thread_.request_stop();
    sender_thread_.request_stop();

    if(watchdog_thread_.joinable()) {
        watchdog_thread_.join();
    }
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

void Manager::process_heartbeat(const message::CHP1Message& msg) {
    LOG(logger_, DEBUG) << msg.getSender() << " reports state " << magic_enum::enum_name(msg.getState())
                        << ", next message in " << msg.getInterval().count();

    // Update or add the remote:
    const auto remote_it = remotes_.find(msg.getSender());
    if(remote_it != remotes_.end()) {

        if(std::chrono::system_clock::now() - msg.getTime() > 3s) {
            // TODO(simonspa) log a warning?
        }

        remote_it->second.interval = msg.getInterval();
        remote_it->second.last_heartbeat = std::chrono::system_clock::now();
        remote_it->second.last_state = msg.getState();

        // replenish lives:
        remote_it->second.lives = 3;
    } else {
        remotes_.emplace(msg.getSender(), Remote {msg.getInterval(), std::chrono::system_clock::now(), msg.getState()});
    }
}

void Manager::start() {
    // jthread immediately starts on construction
    receiver_thread_ = std::jthread(std::bind_front(&HeartbeatRecv::loop, &receiver_));
    sender_thread_ = std::jthread(std::bind_front(&HeartbeatSend::loop, &sender_));
    watchdog_thread_ = std::jthread(std::bind_front(&Manager::run, this));
}

void Manager::run(const std::stop_token& stop_token) {
    std::unique_lock<std::mutex> lock {mutex_};

    while(!stop_token.stop_requested()) {

        // Calculate the next wake-up by checking when the next heartbeat times out, but time out after 3s anyway:
        auto wakeup = std::chrono::system_clock::now() + 3s;
        for(auto& [key, remote] : remotes_) {

            // Check if we are beyond the interval
            if(remote.lives > 0 && std::chrono::system_clock::now() > remote.last_heartbeat + remote.interval) {
                // We have lives left, reduce them by one
                remote.lives--;
                // We have subtracted a live, so let's wait another interval:
                remote.last_heartbeat = std::chrono::system_clock::now();
                LOG(logger_, DEBUG) << "Missed heartbeat from " << key << ", reduced lives to " << remote.lives;

                if(remote.lives == 0) {
                    // This parrot is dead, it is no more
                    LOG(logger_, WARNING) << "Missed heartbeats from " << key << ", no lives left";
                    interrupt_callback_();
                }
            }

            // Update time point until we have to wait:
            wakeup = std::min(wakeup, remote.last_heartbeat + remote.interval);
        }

        cv_.wait_until(lock, wakeup, [&]() { return stop_token.stop_requested(); });
    }
}
