/**
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/protocol/CHP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

#include "chirp_mock.hpp"

class CHPMockSender {
public:
    CHPMockSender(std::string name)
        : name_(std::move(name)), pub_socket_(*constellation::networking::global_zmq_context(), zmq::socket_type::pub),
          port_(constellation::networking::bind_ephemeral_port(pub_socket_)) {}

    constellation::networking::Port getPort() const { return port_; }

    std::string_view getName() const { return name_; }

    void sendHeartbeat(constellation::protocol::CSCP::State state,
                       std::chrono::milliseconds interval,
                       constellation::protocol::CHP::MessageFlags flags = {}) {
        auto msg = constellation::message::CHP1Message(name_, state, interval, flags);
        msg.assemble().send(pub_socket_);
    }

    void sendExtrasystole(constellation::protocol::CSCP::State state,
                          std::chrono::milliseconds interval,
                          constellation::protocol::CHP::MessageFlags flags = {},
                          std::optional<std::string> status = {}) {
        auto msg = constellation::message::CHP1Message(name_, state, interval, flags, std::move(status));
        msg.assemble().send(pub_socket_);
    }

    void mockChirpOffer() { mocked_service_.emplace_back(name_, constellation::protocol::CHIRP::HEARTBEAT, port_); }
    void mockChirpDepart() { mocked_service_.clear(); }

private:
    std::string name_;
    zmq::socket_t pub_socket_;
    constellation::networking::Port port_;
    std::deque<MockedChirpService> mocked_service_;
};

class CHPMockReceiver
    : public constellation::pools::SubscriberPool<constellation::message::CHP1Message,
                                                  constellation::protocol::CHIRP::ServiceIdentifier::HEARTBEAT> {
public:
    using SubscriberPoolT =
        SubscriberPool<constellation::message::CHP1Message, constellation::protocol::CHIRP::ServiceIdentifier::HEARTBEAT>;
    CHPMockReceiver()
        : SubscriberPoolT("LINK", [this](constellation::message::CHP1Message&& msg) {
              const std::lock_guard last_message_lock {last_message_mutex_};
              last_message_ = std::make_shared<constellation::message::CHP1Message>(std::move(msg));
              last_message_updated_.store(true);
          }) {}

    void waitSubscription() {
        while(!subscribed_.load()) {
            std::this_thread::yield();
        }
        subscribed_.store(false);
    }
    void waitNextMessage() {
        while(!last_message_updated_.load()) {
            std::this_thread::yield();
        }
        last_message_updated_.store(false);
    }
    std::shared_ptr<constellation::message::CHP1Message> getLastMessage() {
        const std::lock_guard last_message_lock {last_message_mutex_};
        return last_message_;
    }

protected:
    void host_connected(const constellation::chirp::DiscoveredService& service) final {
        subscribe(service.host_id, "");
        subscribed_.store(true);
    }

private:
    std::atomic_bool subscribed_ {false};
    std::atomic_bool last_message_updated_ {false};
    std::mutex last_message_mutex_;
    std::shared_ptr<constellation::message::CHP1Message> last_message_;
};
