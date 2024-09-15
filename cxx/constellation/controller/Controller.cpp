/**
 * @file
 * @brief Implementation of Controller class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Controller.hpp"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <magic_enum.hpp>
#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CHP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;

Controller::Controller(std::string controller_name)
    : logger_("CTRL"), controller_name_(std::move(controller_name)),
      heartbeat_receiver_([this](auto&& arg) { process_heartbeat(std::forward<decltype(arg)>(arg)); }),
      watchdog_thread_(std::bind_front(&Controller::controller_loop, this)) {
    LOG(logger_, DEBUG) << "Registering controller callback";
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->registerDiscoverCallback(&Controller::callback, chirp::CONTROL, this);
        chirp_manager->sendRequest(chirp::CONTROL);
    }

    // Start heartbeat receiver:
    heartbeat_receiver_.startPool();
}

Controller::~Controller() {
    heartbeat_receiver_.stopPool();

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

void Controller::reached_global_state(CSCP::State /*state*/) {};
void Controller::reached_lowest_state(CSCP::State /*state*/) {};
void Controller::propagate_update(UpdateType /*type*/, std::size_t /*position*/, std::size_t /*total*/) {};

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
            // Note the position of the removed item:
            const auto position = std::distance(connections_.begin(), it);

            it->second.req.close();
            LOG(logger_, DEBUG) << "Satellite " << std::quoted(it->first) << " at " << uri << " departed";
            connections_.erase(it);

            // Trigger method for propagation of connection list updates in derived controller classes
            propagate_update(UpdateType::REMOVED, position, connections_.size());
        }
    } else {
        // New satellite connection
        Connection conn = {{context_, zmq::socket_type::req}, service.host_id, uri};
        conn.req.connect(uri);

        // Obtain canonical name:
        auto send_msg_name = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, "get_name"});
        const auto recv_msg_name = send_receive(conn, send_msg_name);
        const auto name = recv_msg_name.getVerb().second;

        // Obtain current state
        auto send_msg_state = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, "get_state"});
        const auto recv_msg_state = send_receive(conn, send_msg_state);
        conn.state = magic_enum::enum_cast<CSCP::State>(recv_msg_state.getVerb().second).value_or(CSCP::State::NEW);

        // Add to map of open connections
        const auto [it, success] = connections_.emplace(name, std::move(conn));

        if(!success) {
            LOG(logger_, WARNING) << "Not adding remote satellite " << std::quoted(name) << " at " << uri
                                  << ", a satellite with the same canonical name was already registered";
        } else {
            LOG(logger_, DEBUG) << "Registered remote satellite " << std::quoted(name) << " at " << uri;

            // Trigger method for propagation of connection list updates in derived controller classes
            propagate_update(UpdateType::ADDED, std::distance(connections_.begin(), it), connections_.size());
        }
    }
}

void Controller::process_heartbeat(message::CHP1Message&& msg) {

    std::unique_lock<std::mutex> lock {connection_mutex_};
    const auto now = std::chrono::system_clock::now();

    // Find satellite from connection list based in the heartbeat sender name
    const auto sat = connections_.find(msg.getSender());
    if(sat != connections_.end()) {
        LOG(logger_, TRACE) << msg.getSender() << " reports state " << magic_enum::enum_name(msg.getState())
                            << ", next message in " << msg.getInterval().count();

        const auto deviation = std::chrono::duration_cast<std::chrono::seconds>(now - msg.getTime());
        if(std::chrono::abs(deviation) > 3s) [[unlikely]] {
            LOG(logger_, WARNING) << "Detected time deviation of " << deviation << " to " << msg.getSender();
        }

        // Check if a state has changed and we need to calculate and propagate updates:
        const bool state_updated = (sat->second.state != msg.getState());

        // Update status and timers
        sat->second.interval = msg.getInterval();
        sat->second.last_heartbeat = now;
        sat->second.state = msg.getState();

        // Replenish lives unless we're in ERROR or SAFE state:
        if(msg.getState() != CSCP::State::ERROR && msg.getState() != CSCP::State::SAFE) {
            sat->second.lives = protocol::CHP::Lives;
        }

        // A state was changed, propagate this:
        if(state_updated) {
            // Notify derived classes of change
            propagate_update(UpdateType::UPDATED, std::distance(connections_.begin(), sat), connections_.size());

            lock.unlock();
            if(isInGlobalState()) {
                // Notify if this new state is a global one:
                reached_global_state(msg.getState());
            } else {
                // Notify of currently lowest state
                reached_lowest_state(getLowestState());
            }
        }
    } else {
        LOG(logger_, TRACE) << "Ignoring heartbeat from " << msg.getSender() << ", satellite is not connected";
    }
}

std::set<std::string> Controller::getConnections() const {
    const std::lock_guard connection_lock {connection_mutex_};
    std::set<std::string> connections;
    for(const auto& [key, conn] : connections_) {
        connections.insert(key);
    }
    return connections;
}

std::string Controller::getRunIdentifier() {
    const std::lock_guard connection_lock {connection_mutex_};
    for(auto& [name, sat] : connections_) {
        // Obtain run identifier:
        auto send_msg = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, "get_run_id"});
        const auto recv_msg = send_receive(sat, send_msg);
        const auto runid = recv_msg.getVerb().second;
        if(recv_msg.getVerb().first == CSCP1Message::Type::SUCCESS && !runid.empty()) {
            return to_string(runid);
        }
    }
    return {};
}

std::optional<std::chrono::system_clock::time_point> Controller::getRunStartTime() {
    const std::lock_guard connection_lock {connection_mutex_};

    std::optional<std::chrono::system_clock::time_point> time {};
    for(auto& [name, sat] : connections_) {
        // Obtain run starting time from get_state command metadata:
        auto send_msg = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, "get_state"});
        const auto recv_msg = send_receive(sat, send_msg);

        try {
            const auto state = magic_enum::enum_cast<CSCP::State>(recv_msg.getVerb().second).value_or(CSCP::State::NEW);
            const auto& header = recv_msg.getHeader();
            if(state == CSCP::State::RUN && header.hasTag("last_changed")) {
                const auto timestamp = header.getTag<std::chrono::system_clock::time_point>("last_changed");
                LOG(logger_, DEBUG) << "Run started for " << std::quoted(header.getSender()) << " at "
                                    << utils::to_string(timestamp);
                // Use latest available timestamp:
                time = std::max(timestamp, time.value_or(timestamp));
            }
        } catch(const msgpack::unpack_error&) {
            continue;
        }
    }
    return time;
}

bool Controller::isInState(CSCP::State state) const {
    const std::lock_guard connection_lock {connection_mutex_};

    return std::ranges::all_of(
        connections_.cbegin(), connections_.cend(), [state](const auto& conn) { return conn.second.state == state; });
}

bool Controller::isInGlobalState() const {
    const std::lock_guard connection_lock {connection_mutex_};

    // If no adjacent connection with different state found, then in global state
    return std::ranges::adjacent_find(connections_.cbegin(), connections_.cend(), [](auto const& x, auto const& y) {
               return x.second.state != y.second.state;
           }) == connections_.cend();
}

CSCP::State Controller::getLowestState() const {
    const std::lock_guard connection_lock {connection_mutex_};

    if(connections_.empty()) {
        return CSCP::State::NEW;
    }

    return std::ranges::min_element(connections_.cbegin(),
                                    connections_.cend(),
                                    [](auto const& x, auto const& y) { return x.second.state < y.second.state; })
        ->second.state;
}

CSCP1Message Controller::send_receive(Connection& conn, CSCP1Message& cmd, bool keep_payload) const {

    // Check if this is a request message
    if(cmd.getVerb().first != CSCP1Message::Type::REQUEST) {
        return {{controller_name_}, {CSCP1Message::Type::ERROR, "Can only send command messages of type REQUEST"}};
    }

    // Possible keep payload, we might send multiple command messages:
    cmd.assemble(keep_payload).send(conn.req);
    zmq::multipart_t recv_zmq_msg {};
    recv_zmq_msg.recv(conn.req);
    return CSCP1Message::disassemble(recv_zmq_msg);
}

CSCP1Message Controller::build_message(std::string verb, const CommandPayload& payload) const {
    auto send_msg = CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, std::move(verb)});
    if(std::holds_alternative<Dictionary>(payload)) {
        send_msg.addPayload(std::get<Dictionary>(payload).assemble());
    } else if(std::holds_alternative<List>(payload)) {
        send_msg.addPayload(std::get<List>(payload).assemble());
    } else if(std::holds_alternative<std::string>(payload)) {
        msgpack::sbuffer sbuf {};
        msgpack::pack(sbuf, std::get<std::string>(payload));
        send_msg.addPayload(std::move(sbuf));
    }
    return send_msg;
}

CSCP1Message Controller::sendCommand(std::string_view satellite_name, CSCP1Message& cmd) {
    const std::lock_guard connection_lock {connection_mutex_};

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

CSCP1Message Controller::sendCommand(std::string_view satellite_name, std::string verb, const CommandPayload& payload) {
    auto send_msg = build_message(std::move(verb), payload);
    return sendCommand(satellite_name, send_msg);
}

std::map<std::string, CSCP1Message> Controller::sendCommands(CSCP1Message& cmd) {

    const std::lock_guard connection_lock {connection_mutex_};
    std::map<std::string, CSCP1Message> replies {};
    for(auto& [name, sat] : connections_) {
        replies.emplace(name, send_receive(sat, cmd, true));

        // Update last command info
        auto verb = replies.at(name).getVerb();
        sat.last_cmd_type = verb.first;
        sat.last_cmd_verb = verb.second;
    }
    return replies;
}

std::map<std::string, CSCP1Message> Controller::sendCommands(std::string verb, const CommandPayload& payload) {
    auto send_msg = build_message(std::move(verb), payload);
    return sendCommands(send_msg);
}

std::map<std::string, CSCP1Message> Controller::sendCommands(const std::string& verb,
                                                             const std::map<std::string, CommandPayload>& payloads) {

    const std::lock_guard connection_lock {connection_mutex_};
    std::map<std::string, CSCP1Message> replies {};
    for(auto& [name, sat] : connections_) {
        // Prepare message:
        auto send_msg = (payloads.contains(name) ? build_message(verb, payloads.at(name))
                                                 : CSCP1Message({controller_name_}, {CSCP1Message::Type::REQUEST, verb}));

        // Send command and receive reply:
        replies.emplace(name, send_receive(sat, send_msg));

        // Update last command info
        auto verb = replies.at(name).getVerb();
        sat.last_cmd_type = verb.first;
        sat.last_cmd_verb = verb.second;
    }
    return replies;
}

void Controller::controller_loop(const std::stop_token& stop_token) {
    std::unique_lock<std::mutex> lock {connection_mutex_};
    auto wakeup = std::chrono::system_clock::now() + 3s;

    // Wait until cv is notified, timeout is reached or stop is requested, returns true if stop requested
    while(!cv_.wait_until(lock, stop_token, wakeup, [&]() { return stop_token.stop_requested(); })) {

        // Calculate the next wake-up by checking when the next heartbeat times out, but time out after 3s anyway:
        wakeup = std::chrono::system_clock::now() + 3s;

        for(auto conn = connections_.begin(), next_conn = conn; conn != connections_.end(); conn = next_conn) {
            ++next_conn;

            auto& [key, remote] = *conn;

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

                    // Note position of removed item:
                    const auto position = std::distance(connections_.begin(), conn);

                    // Close connection, remove from list:
                    remote.req.close();
                    connections_.erase(conn);

                    // Trigger method for propagation of connection list updates in derived controller classes
                    propagate_update(UpdateType::REMOVED, position, connections_.size());
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