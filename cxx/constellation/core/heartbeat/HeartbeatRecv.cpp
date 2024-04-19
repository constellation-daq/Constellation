/**
 * @file
 * @brief CMDP log receiver
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "HeartbeatRecv.hpp"

#include <any>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std23.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation;
using namespace constellation::heartbeat;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;

HeartbeatRecv::HeartbeatRecv() : logger_("CHP") {
    // Register callback
    chirp::Manager::getDefaultInstance()->registerDiscoverCallback(&HeartbeatRecv::callback, chirp::HEARTBEAT, this);
    // Request currently active logging services
    chirp::Manager::getDefaultInstance()->sendRequest(chirp::HEARTBEAT);
}

void HeartbeatRecv::connect(chirp::DiscoveredService service) {
    const std::lock_guard sockets_lock {sockets_mutex_};
    const auto uri = "tcp://" + service.address.to_string() + ":" + std::to_string(service.port);

    // Connect
    LOG(logger_, DEBUG) << "Connecting to " << uri << "...";
    zmq::socket_t socket {context_, zmq::socket_type::sub};
    socket.connect(uri);
    socket.set(zmq::sockopt::subscribe, "");

    // Register with poller:
    zmq::active_poller_t::handler_type handler = [this, sock = zmq::socket_ref(socket)](zmq::event_flags ef) {
        if((ef & zmq::event_flags::pollin) != zmq::event_flags::none) {
            zmq::multipart_t zmq_msg {};
            auto received = zmq_msg.recv(sock);
            if(received) {

                try {
                    auto chp_msg = CHP1Message::disassemble(zmq_msg);
                    LOG(logger_, INFO) << chp_msg.getSender() << " reports state "
                                       << magic_enum::enum_name(chp_msg.getState()) << ", next message in "
                                       << chp_msg.getInterval().count();
                } catch(const MessageDecodingError& error) {
                    LOG(logger_, WARNING) << error.what();
                } catch(const IncorrectMessageType& error) {
                    LOG(logger_, WARNING) << error.what();
                }
            }
        }
    };

    poller_.add(socket, zmq::event_flags::pollin, handler);

    sockets_.insert(std::make_pair(service, std::move(socket)));
    LOG(logger_, INFO) << "Connected to " << uri;
}

void HeartbeatRecv::disconnect(chirp::DiscoveredService service) {
    const std::lock_guard sockets_lock {sockets_mutex_};
    const auto uri = "tcp://" + service.address.to_string() + ":" + std::to_string(service.port);

    // Disconnect the socket
    const auto socket_it = sockets_.find(service);
    if(socket_it != sockets_.end()) {
        LOG(logger_, DEBUG) << "Disconnecting from " << uri << "...";
        // Remove from poller
        poller_.remove(zmq::socket_ref(socket_it->second));

        socket_it->second.disconnect(uri);
        socket_it->second.close();

        sockets_.erase(socket_it);
        LOG(logger_, INFO) << "Disconnected from " << uri;
    }
}

void HeartbeatRecv::callback_impl(chirp::DiscoveredService service, bool depart) {
    const auto uri = "tcp://" + service.address.to_string() + ":" + std::to_string(service.port);
    LOG(logger_, TRACE) << "Callback for " << uri << (depart ? ", departing" : "");

    if(depart) {
        disconnect(service);
    } else {
        connect(service);
    }

    // Ping the main thread
    cv_.notify_one();
}

void HeartbeatRecv::callback(chirp::DiscoveredService service, bool depart, std::any user_data) {
    auto* instance = std::any_cast<HeartbeatRecv*>(user_data);
    instance->callback_impl(std::move(service), depart);
}

void HeartbeatRecv::main_loop() {
    while(true) {
        std::unique_lock<std::mutex> lock(sockets_mutex_);
        cv_.wait(lock, [this] { return !sockets_.empty(); });

        lock.unlock();
        poller_.wait(1000ms);
    }
}
