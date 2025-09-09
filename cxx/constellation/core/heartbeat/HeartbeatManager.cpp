/**
 * @file
 * @brief Implementation of the heartbeat manager
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "HeartbeatManager.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/heartbeat/HeartbeatRecv.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/protocol/CHP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/thread.hpp"

using namespace constellation::heartbeat;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::chrono_literals;

HeartbeatManager::HeartbeatManager(std::string sender,
                                   std::function<CSCP::State()> state_callback,
                                   std::function<void(std::string_view)> interrupt_callback,
                                   std::function<void(std::string_view)> degradation_callback)
    : HeartbeatRecv([this](auto&& arg) { process_heartbeat(std::forward<decltype(arg)>(arg)); }),
      sender_(std::move(sender), std::move(state_callback), CHP::MaximumInterval), role_(CHP::Role::DYNAMIC),
      interrupt_callback_(std::move(interrupt_callback)), degradation_callback_(std::move(degradation_callback)),
      logger_("LINK"), watchdog_thread_(std::bind_front(&HeartbeatManager::run, this)) {
    set_thread_name(watchdog_thread_, "HeartbeatManager");
    startPool();
}

HeartbeatManager::~HeartbeatManager() {
    terminate();
}

void HeartbeatManager::terminate() {
    // Stop heartbeat manager pool and watchdog thread
    stopPool();
    watchdog_thread_.request_stop();
    if(watchdog_thread_.joinable()) {
        watchdog_thread_.join();
    }
    // Stop heartbeat sender thread and unregister CHIRP service
    sender_.terminate();
}

void HeartbeatManager::sendExtrasystole(std::string status) {
    sender_.sendExtrasystole(std::move(status));
}

std::optional<CSCP::State> HeartbeatManager::getRemoteState(std::string_view remote) {
    const std::lock_guard lock {mutex_};
    const auto remote_it = std::ranges::find_if(
        remotes_, [remote](const auto& r) { return transform(r.first, ::tolower) == transform(remote, ::tolower); });
    if(remote_it != remotes_.end()) {
        // If the remote has vanished, return ERROR state
        if(remote_it->second.lives == 0) {
            return CSCP::State::ERROR;
        }

        return remote_it->second.last_state;
    }

    // Remote unknown, return empty optional
    return {};
}

void HeartbeatManager::setRole(CHP::Role role) {
    sender_.setFlags(CHP::flags_from_role(role));
    role_.store(role);
}

void HeartbeatManager::host_disconnected(const chirp::DiscoveredService& service) {

    LOG(logger_, DEBUG) << "Processing orderly departure of remote " << service.to_uri();
    const std::lock_guard lock {mutex_};

    // Remove the remote
    auto remote_it =
        std::ranges::find_if(remotes_, [&service](const auto& remote) { return MD5Hash(remote.first) == service.host_id; });
    if(remote_it != remotes_.end()) {
        // Check if the run needs to be marked as degraded
        if(degradation_callback_ && role_requires(remote_it->second.role, CHP::MessageFlags::MARK_DEGRADED)) {
            degradation_callback_(quote(remote_it->first) + " departed illicitly");
        }

        // Check if per its role, this remote is allowed to depart:
        if(interrupt_callback_ && role_requires(remote_it->second.role, CHP::MessageFlags::DENY_DEPARTURE)) {
            LOG(logger_, DEBUG) << quote(remote_it->first) << " departed with " << "DENY_DEPARTURE"_quote
                                << " flag, requesting interrupt";
            interrupt_callback_(quote(remote_it->first) + " departed illicitly");
        } else {
            LOG(INFO) << quote(remote_it->first) << " departed orderly";
        }
        remotes_.erase(remote_it);
    }
}

void HeartbeatManager::process_heartbeat(const CHP1Message& msg) {
    const auto& status = msg.getStatus();
    LOG(logger_, TRACE) << quote(msg.getSender()) << " reports state " << msg.getState()   //
                        << ", flags " << enum_name(msg.getFlags())                         //
                        << (status.has_value() ? ", status " + quote(status.value()) : "") //
                        << ", next message in " << msg.getInterval();                      //

    const auto now = std::chrono::system_clock::now();
    std::unique_lock<std::mutex> lock {mutex_};

    // Update or add the remote:
    auto remote_it = remotes_.find(msg.getSender());

    // Add newly discovered remote:
    if(remote_it == remotes_.end()) {
        LOG(logger_, DEBUG) << "Adding " << quote(msg.getSender()) << " after receiving first heartbeat";
        auto [it, inserted] =
            remotes_.emplace(msg.getSender(), Remote(msg.getRole(), msg.getInterval(), now, msg.getState(), now));
        remote_it = it;
    }

    // Check for time deviation
    const auto deviation = std::chrono::duration_cast<std::chrono::seconds>(now - msg.getTime());
    if(std::chrono::abs(deviation) > 3s) [[unlikely]] {
        LOG(logger_, DEBUG) << "Detected time deviation of " << deviation << " to " << quote(msg.getSender());
    }

    // Update the role with latest information:
    remote_it->second.role = msg.getRole();

    bool call_interrupt = false;
    // Check for ERROR and SAFE states:
    if(remote_it->second.lives > 0 && (msg.getState() == CSCP::State::ERROR || msg.getState() == CSCP::State::SAFE)) {
        remote_it->second.lives = 0;
        // Only trigger interrupt if demanded by the message flags:
        call_interrupt = (interrupt_callback_ && msg.hasFlag(CHP::MessageFlags::TRIGGER_INTERRUPT));
    }

    // Update remote
    remote_it->second.interval = msg.getInterval();
    remote_it->second.last_heartbeat = now;
    remote_it->second.last_state = msg.getState();

    // Replenish lives unless we're in ERROR or SAFE state:
    if(msg.getState() != CSCP::State::ERROR && msg.getState() != CSCP::State::SAFE) {
        remote_it->second.lives = CHP::Lives;
    }

    const auto remote_name = remote_it->first;

    // Delay calling the interrupt until we have unlocked the mutex:
    lock.unlock();
    if(call_interrupt) {
        LOG(logger_, DEBUG) << "Detected state " << msg.getState() << " at " << quote(remote_name) << ", interrupting";
        interrupt_callback_(quote(remote_name) + " reports state " + to_string(msg.getState()));
    }
}

void HeartbeatManager::run(const std::stop_token& stop_token) {
    // Notify condition variable when stop is requested
    const std::stop_callback stop_callback {stop_token, [&]() { cv_.notify_all(); }};

    auto wakeup = std::chrono::system_clock::now() + 3s;

    while(!stop_token.stop_requested()) {
        std::unique_lock<std::mutex> lock {mutex_};
        // Wait until condition variable is notified or timeout is reached
        cv_.wait_until(lock, wakeup);

        // Calculate the next wake-up by checking when the next heartbeat times out, but time out after 3s anyway:
        wakeup = std::chrono::system_clock::now() + 3s;
        for(auto& [key, remote] : remotes_) {
            // Check if we are beyond the interval and that we only subtract lives once every interval
            const auto now = std::chrono::system_clock::now();
            if(remote.lives > 0 && now - remote.last_heartbeat > remote.interval &&
               now - remote.last_checked > remote.interval) {
                // We have lives left, reduce them by one
                remote.lives--;
                remote.last_checked = now;
                LOG(logger_, TRACE) << "Missed heartbeat from " << quote(key) << ", reduced lives to "
                                    << to_string(remote.lives);

                if(remote.lives == 0) {
                    const auto msg = "No signs of life detected anymore from " + quote(key);
                    LOG(logger_, WARNING) << msg;

                    // Check if the run needs to be marked as degraded
                    if(degradation_callback_ && role_requires(remote.role, CHP::MessageFlags::MARK_DEGRADED)) {
                        degradation_callback_(msg);
                    }

                    // Only trigger interrupt if the role demands it
                    if(interrupt_callback_ && role_requires(remote.role, CHP::MessageFlags::TRIGGER_INTERRUPT)) {
                        // This parrot is dead, it is no more
                        interrupt_callback_(msg);
                    }
                }
            }

            // Update time point until we have to wait (if not in the past)
            const auto next_heartbeat = remote.last_heartbeat + remote.interval;
            if(next_heartbeat - now > std::chrono::system_clock::duration::zero()) {
                wakeup = std::min(wakeup, next_heartbeat);
            }
            LOG(logger_, TRACE) << "Updated heartbeat wakeup timer to "
                                << std::chrono::duration_cast<std::chrono::milliseconds>(wakeup - now);
        }
    }
}
