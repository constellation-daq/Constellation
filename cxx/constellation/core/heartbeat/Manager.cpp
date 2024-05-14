/**
 * @file
 * @brief Implementation of the heartbeat manager
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

HeartbeatManager::HeartbeatManager(std::string_view sender)
    : receiver_([this](auto&& arg) { process_heartbeat(std::forward<decltype(arg)>(arg)); }),
      sender_(std::string(sender), 1000ms), logger_("CHP"), watchdog_thread_(std::bind_front(&HeartbeatManager::run, this)) {
}

HeartbeatManager::~HeartbeatManager() {
    watchdog_thread_.request_stop();
    cv_.notify_one();

    if(watchdog_thread_.joinable()) {
        watchdog_thread_.join();
    }
}

void HeartbeatManager::updateState(State state) {
    sender_.updateState(state);
}

std::optional<State> HeartbeatManager::getRemoteState(std::string_view remote) {
    const auto remote_it = remotes_.find(remote);
    if(remote_it != remotes_.end()) {
        return remote_it->second.last_state;
    }

    // Remote unknown, return empty optional
    return {};
}

void HeartbeatManager::process_heartbeat(const message::CHP1Message& msg) {
    LOG(logger_, TRACE) << msg.getSender() << " reports state " << magic_enum::enum_name(msg.getState())
                        << ", next message in " << msg.getInterval().count();

    // Update or add the remote:
    const auto remote_it = remotes_.find(msg.getSender());
    if(remote_it != remotes_.end()) {

        if(std::chrono::system_clock::now() - msg.getTime() > 3s) {
            LOG(logger_, WARNING) << "Detected time deviation of "
                                  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() -
                                                                                           msg.getTime())
                                         .count()
                                  << "ms to " << msg.getSender();
        }

        remote_it->second.interval = msg.getInterval();
        remote_it->second.last_heartbeat = std::chrono::system_clock::now();
        remote_it->second.last_state = msg.getState();

        // Replenish lives unless we're in error state:
        if(msg.getState() != State::ERROR) {
            remote_it->second.lives = 3;
        }
    } else {
        remotes_.emplace(msg.getSender(), Remote {msg.getInterval(), std::chrono::system_clock::now(), msg.getState()});
    }
}

void HeartbeatManager::run(const std::stop_token& stop_token) {
    std::unique_lock<std::mutex> lock {mutex_};

    while(!stop_token.stop_requested()) {

        // Calculate the next wake-up by checking when the next heartbeat times out, but time out after 3s anyway:
        auto wakeup = std::chrono::system_clock::now() + 3s;
        for(auto& [key, remote] : remotes_) {
            // Check for ERROR states:
            if(remote.lives > 0 && remote.last_state == State::ERROR) {
                remote.lives = 0;
                if(interrupt_callback_) {
                    LOG(logger_, DEBUG) << "Detected state " << magic_enum::enum_name(remote.last_state) << " at " << key
                                        << ", interrupting";
                    interrupt_callback_();
                }
            }

            // Check if we are beyond the interval
            if(remote.lives > 0 && std::chrono::system_clock::now() > remote.last_heartbeat + remote.interval) {
                // We have lives left, reduce them by one
                remote.lives--;
                // We have subtracted a live, so let's wait another interval:
                remote.last_heartbeat = std::chrono::system_clock::now();
                LOG(logger_, TRACE) << "Missed heartbeat from " << key << ", reduced lives to " << remote.lives;

                if(remote.lives == 0 && interrupt_callback_) {
                    // This parrot is dead, it is no more
                    LOG(logger_, DEBUG) << "Missed heartbeats from " << key << ", no lives left";
                    interrupt_callback_();
                }
            }

            // Update time point until we have to wait:
            wakeup = std::min(wakeup, remote.last_heartbeat + remote.interval);
        }

        cv_.wait_until(lock, wakeup, [&]() { return stop_token.stop_requested(); });
    }
}
