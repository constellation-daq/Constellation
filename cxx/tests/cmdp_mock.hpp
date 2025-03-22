/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"

#include "chirp_mock.hpp"

class CMDPSender {
public:
    CMDPSender(std::string name)
        : name_(std::move(name)), pub_socket_(*constellation::networking::global_zmq_context(), zmq::socket_type::xpub),
          port_(constellation::networking::bind_ephemeral_port(pub_socket_)) {}

    constellation::networking::Port getPort() const { return port_; }

    std::string_view getName() const { return name_; }

    void sendLogMessage(constellation::log::Level level, std::string topic, std::string message) {
        auto msg = constellation::message::CMDP1LogMessage(level, std::move(topic), {name_}, std::move(message));
        msg.assemble().send(pub_socket_);
    }

    void sendStatMessage(std::string name,
                         std::string unit,
                         constellation::metrics::MetricType type,
                         constellation::config::Value&& value) {
        auto msg = constellation::message::CMDP1StatMessage(
            {name_},
            {std::make_shared<constellation::metrics::Metric>(std::move(name), std::move(unit), type), std::move(value)});
        msg.assemble().send(pub_socket_);
    }

    void sendNotification(std::string id, constellation::config::Dictionary topics) {
        auto msg = constellation::message::CMDP1Notification({name_}, std::move(id), std::move(topics));
        msg.assemble().send(pub_socket_);
    }

    void sendRaw(zmq::multipart_t& msg) { msg.send(pub_socket_); }

    zmq::multipart_t recv() {
        zmq::multipart_t recv_msg {};
        recv_msg.recv(pub_socket_);
        return recv_msg;
    }

    bool canRecv() {
        zmq::message_t msg {};
        pub_socket_.set(zmq::sockopt::rcvtimeo, 200);
        auto recv_res = pub_socket_.recv(msg);
        pub_socket_.set(zmq::sockopt::rcvtimeo, -1);
        return recv_res.has_value();
    }

    void mockChirpService() { mocked_service_.emplace_back(name_, constellation::protocol::CHIRP::MONITORING, port_); }

private:
    std::string name_;
    zmq::socket_t pub_socket_;
    constellation::networking::Port port_;
    std::deque<MockedChirpService> mocked_service_;
};

inline bool check_sub_message(zmq::message_t msg, bool subscribe, std::string_view topic) {
    // First byte is subscribe bool
    const auto msg_subscribe = static_cast<bool>(*msg.data<std::uint8_t>());
    if(msg_subscribe != subscribe) {
        return false;
    }
    // Rest is subscription topic
    auto msg_topic = msg.to_string_view();
    msg_topic.remove_prefix(1);
    return msg_topic == topic;
}
