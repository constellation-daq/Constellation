/**
 * @file
 * @brief Implementation of Controller class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Controller.hpp"

#include <string>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/fsm_definitions.hpp"

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;

Controller::Controller(std::string_view controller_name)
    : logger_("CONTROLLER"), controller_name_(controller_name),
      heartbeat_receiver_([this](auto&& arg) { process_heartbeat(std::forward<decltype(arg)>(arg)); }),
      watchdog_thread_(std::bind_front(&Controller::run, this)) {
    LOG(logger_, DEBUG) << "Registering controller callback";
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->registerDiscoverCallback(&Controller::callback, chirp::CONTROL, this);
        chirp_manager->sendRequest(chirp::CONTROL);
    }
}

Controller::~Controller() {

    // Unregister callback
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->unregisterDiscoverCallback(&Controller::callback, chirp::CONTROL);
    }

    // Close all open connections
    const std::lock_guard connection_lock {connection_mutex_};
    for(auto& conn : connections_) {
        conn.second.req.close();
    }
    connections_.clear();
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
void Controller::callback(chirp::DiscoveredService service, bool depart, std::any user_data) {
    auto* instance = std::any_cast<Controller*>(user_data);
    instance->callback_impl(service, depart);
}

void Controller::callback_impl(const constellation::chirp::DiscoveredService& service, bool depart) {

    const std::lock_guard connection_lock {connection_mutex_};

    // Add or drop, depending on message:
    const auto uri = service.to_uri();
    if(depart) {
        const auto it = std::find_if(connections_.begin(), connections_.end(), [&](const auto& sat) {
            return sat.second.host_id == service.host_id;
        });
        if(it != connections_.end()) {
            it->second.req.close();
            connections_.erase(it);
            LOG(logger_, INFO) << "Satellite at " << uri << " departed";
        }
    } else {
        // New satellite connection
        Connection conn = {{context_, zmq::socket_type::req}, service.host_id};
        conn.req.connect(uri);

        // Obtain canonical name:
        auto send_msg_name = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, "get_name"});
        const auto recv_msg_name = send_receive(conn, send_msg_name);
        const auto name = recv_msg_name.getVerb().second;

        // Obtain current state
        auto send_msg_state = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, "get_state"});
        const auto recv_msg_state = send_receive(conn, send_msg_state);
        conn.state = magic_enum::enum_cast<State>(recv_msg_state.getVerb().second).value_or(State::NEW);

        // Add to map of open connections
        const auto [it, success] = connections_.emplace(name, std::move(conn));
        if(!success) {
            LOG(logger_, DEBUG) << "Not adding remote satellite at " << uri << ", was already registered";
        } else {
            LOG(logger_, INFO) << "Registered remote satellite at " << uri;
        }
    }

    // Trigger method for propagation of connection list updates
    propagate_update(connections_.size());
}

void Controller::process_heartbeat(const message::CHP1Message& msg) {

    const std::lock_guard connection_lock {connection_mutex_};
    const auto now = std::chrono::system_clock::now();

    // Find satellite from connection list based in the heartbeat sender name
    const auto sat = connections_.find(msg.getSender());
    if(sat != connections_.end()) {
        LOG(logger_, DEBUG) << msg.getSender() << " reports state " << magic_enum::enum_name(msg.getState())
                            << ", next message in " << msg.getInterval().count();

        const auto deviation = std::chrono::duration_cast<std::chrono::seconds>(now - msg.getTime());
        if(std::chrono::abs(deviation) > 3s) [[unlikely]] {
            LOG(logger_, WARNING) << "Detected time deviation of " << deviation << " to " << msg.getSender();
        }

        // Update status and timers
        sat->second.interval = msg.getInterval();
        sat->second.last_heartbeat = now;
        sat->second.state = msg.getState();

        // Replenish lives unless we're in ERROR or SAFE state:
        if(msg.getState() != State::ERROR && msg.getState() != State::SAFE) {
            sat->second.lives = default_lives;
        }

        // Call update propagator
        propagate_update(connections_.size());
    } else {
        LOG(logger_, TRACE) << "Ignoring heartbeat from " << msg.getSender() << ", satellite is not connected";
    }
}

bool Controller::isInState(State state) const {
    const std::lock_guard connection_lock {connection_mutex_};

    return std::ranges::all_of(
        connections_.cbegin(), connections_.cend(), [state](const auto& conn) { return conn.second.state == state; });
}

State Controller::getLowestState() const {
    const std::lock_guard connection_lock {connection_mutex_};

    if(connections_.empty()) {
        return State::NEW;
    }

    State state {State::ERROR};
    for(const auto& conn : connections_) {
        if(conn.second.state < state) {
            state = conn.second.state;
        }
    }
    return state;
}

CSCP1Message Controller::send_receive(Connection& conn, CSCP1Message& cmd, bool keep_payload) {
    // Possible keep payload, we might send multiple command messages:
    cmd.assemble(keep_payload).send(conn.req);
    zmq::multipart_t recv_zmq_msg {};
    recv_zmq_msg.recv(conn.req);
    return CSCP1Message::disassemble(recv_zmq_msg);
}

CSCP1Message Controller::sendCommand(std::string_view satellite_name, CSCP1Message& cmd) {
    const std::lock_guard connection_lock {connection_mutex_};

    // Check if this is a request message
    if(cmd.getVerb().first != CSCP1Message::Type::REQUEST) {
        return {{controller_name_}, {CSCP1Message::Type::ERROR, "Can only send command messages of type REQUEST"}};
    }

    // Find satellite by canonical name:
    const auto sat = connections_.find(satellite_name);
    if(sat == connections_.end()) {
        return {{controller_name_}, {CSCP1Message::Type::ERROR, "Target satellite is unknown to controller"}};
    }

    // Exchange messages
    auto response = send_receive(sat->second, cmd);

    // Update last command info
    auto verb = response.getVerb();
    sat->second.last_cmd_type = verb.first;
    sat->second.last_cmd_verb = verb.second;

    return response;
}

std::map<std::string, CSCP1Message> Controller::sendCommands(CSCP1Message& cmd) {

    const std::lock_guard connection_lock {connection_mutex_};
    std::map<std::string, CSCP1Message> replies;
    for(auto& sat : connections_) {
        replies.emplace(sat.first, send_receive(sat.second, cmd, true));

        // Update last command info
        auto verb = replies.at(sat.first).getVerb();
        sat.second.last_cmd_type = verb.first;
        sat.second.last_cmd_verb = verb.second;
    }
    return replies;
}

std::map<std::string, CSCP1Message> Controller::sendCommands(const std::string& verb, const CommandPayload& payload) {
    auto send_msg = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, {verb}});
    if(std::holds_alternative<Dictionary>(payload)) {
        send_msg.addPayload(std::get<Dictionary>(payload).assemble());
    } else if(std::holds_alternative<List>(payload)) {
        send_msg.addPayload(std::get<List>(payload).assemble());
    } else if(std::holds_alternative<std::string>(payload)) {
        msgpack::sbuffer sbuf {};
        msgpack::pack(sbuf, std::get<std::string>(payload));
        send_msg.addPayload(std::move(sbuf));
    }
    return sendCommands(send_msg);
}

CSCP1Message Controller::sendCommand(std::string_view satellite_name,
                                     const std::string& verb,
                                     const CommandPayload& payload) {
    auto send_msg = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, {verb}});
    if(std::holds_alternative<Dictionary>(payload)) {
        send_msg.addPayload(std::get<Dictionary>(payload).assemble());
    } else if(std::holds_alternative<List>(payload)) {
        send_msg.addPayload(std::get<List>(payload).assemble());
    } else if(std::holds_alternative<std::string>(payload)) {
        msgpack::sbuffer sbuf {};
        msgpack::pack(sbuf, std::get<std::string>(payload));
        send_msg.addPayload(std::move(sbuf));
    }
    return sendCommand(satellite_name, send_msg);
}

void Controller::run(const std::stop_token& stop_token) {
    std::unique_lock<std::mutex> lock {connection_mutex_};

    while(!stop_token.stop_requested()) {

        // Calculate the next wake-up by checking when the next heartbeat times out, but time out after 3s anyway:
        auto wakeup = std::chrono::system_clock::now() + 3s;
        for(auto& [key, remote] : connections_) {
            // Check if we are beyond the interval and that we only subtract lives once every interval
            const auto now = std::chrono::system_clock::now();
            if(remote.lives > 0 && now - remote.last_heartbeat > remote.interval &&
               now - remote.last_checked > remote.interval) {
                // We have lives left, reduce them by one
                remote.lives--;
                remote.last_checked = now;
                LOG(logger_, TRACE) << "Missed heartbeat from " << key << ", reduced lives to " << to_string(remote.lives);

                if(remote.lives == 0) {
                    // This parrot is dead, it is no more
                    LOG(logger_, DEBUG) << "Missed heartbeats from " << key << ", no lives left";

                    // Close connection, remove from list:
                    remote.req.close();
                    connections_.erase(key);

                    // Call update propagator
                    propagate_update(connections_.size());
                }
            }

            // Update time point until we have to wait (if not in the past)
            const auto next_heartbeat = remote.last_heartbeat + remote.interval;
            if(next_heartbeat - now > std::chrono::system_clock::duration::zero()) {
                wakeup = std::min(wakeup, next_heartbeat);
            }
            LOG(logger_, TRACE) << "Updated heartbeat wakeup timer to " << (wakeup - now);
        }

        cv_.wait_until(lock, wakeup);
    }
}
