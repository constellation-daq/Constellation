/**
 * @file
 * @brief Implementation of CMDP1 message type
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CMDP1Message.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::metrics;
using namespace constellation::utils;
using namespace std::string_literals;

CMDP1Message::CMDP1Message(std::string topic, CMDP1Message::Header header, message::PayloadBuffer&& payload)
    : topic_(std::move(topic)), header_(std::move(header)), payload_(std::move(payload)) {}

bool CMDP1Message::isLogMessage() const {
    return topic_.starts_with("LOG/");
}

bool CMDP1Message::isStatMessage() const {
    return topic_.starts_with("STAT/");
}

bool CMDP1Message::isNotification() const {
    return topic_.starts_with("STAT?") || topic_.starts_with("LOG?");
}

zmq::multipart_t CMDP1Message::assemble() {
    zmq::multipart_t frames {};

    // First frame: topic
    frames.addstr(topic_);

    // Second frame: header
    msgpack::sbuffer sbuf_header {};
    msgpack_pack(sbuf_header, header_);
    frames.add(PayloadBuffer(std::move(sbuf_header)).to_zmq_msg_release());

    // Third frame: move payload
    frames.add(payload_.to_zmq_msg_release());

    return frames;
}

CMDP1Message CMDP1Message::disassemble(zmq::multipart_t& frames) {
    if(frames.size() != 3) {
        throw MessageDecodingError("CMDP1", "Invalid number of message frames");
    }

    // Decode topic
    const auto topic = frames.pop().to_string();
    if(!(topic.starts_with("LOG/") || topic.starts_with("STAT/") || topic.starts_with("LOG?") ||
         topic.starts_with("STAT?"))) {
        throw MessageDecodingError("CMDP1", "Invalid message topic " + quote(topic) + ", neither log nor telemetry message");
    }

    // Check if valid log level by trying to decode it
    if(topic.starts_with("LOG/")) {
        get_log_level_from_topic(topic);
    }

    // Decode header
    const auto header_frame = frames.pop();
    const auto header = Header::disassemble({to_byte_ptr(header_frame.data()), header_frame.size()});

    // Decode payload
    message::PayloadBuffer payload = {frames.pop()};

    // Create message
    return {topic, header, std::move(payload)};
}

std::string CMDP1Message::getTopic() const {
    if(topic_.starts_with("LOG/")) {
        // Search for second slash after "LOG/" to get substring with log topic
        const auto level_endpos = topic_.find_first_of('/', 4);
        return topic_.substr(level_endpos + 1);
    }
    if(topic_.starts_with("STAT/")) {
        return topic_.substr(5);
    }

    throw IncorrectMessageType("Neither log nor stat message");
}

Level CMDP1Message::get_log_level_from_topic(std::string_view topic) {
    if(!topic.starts_with("LOG/")) {
        throw IncorrectMessageType("Not a log message");
    }

    // Search for second slash after "LOG/" to get substring with log level
    const auto level_endpos = topic.find_first_of('/', 4);
    const auto level_str = topic.substr(4, level_endpos - 4);
    const auto level_opt = enum_cast<Level>(level_str);
    if(!level_opt.has_value()) {
        throw MessageDecodingError("CMDP1", quote(to_string(level_str)) + " is not a valid log level");
    }

    return level_opt.value();
}

CMDP1LogMessage::CMDP1LogMessage(Level level, std::string log_topic, CMDP1Message::Header header, std::string message)
    : CMDP1Message("LOG/" + to_string(level) + (log_topic.empty() ? ""s : "/" + transform(log_topic, ::toupper)),
                   std::move(header),
                   std::move(message)),
      level_(level), log_topic_(std::move(log_topic)) {}

CMDP1LogMessage::CMDP1LogMessage(CMDP1Message&& message) : CMDP1Message(std::move(message)) {
    if(!isLogMessage()) [[unlikely]] {
        throw IncorrectMessageType("Not a log message");
    }

    const auto topic = getMessageTopic();
    level_ = get_log_level_from_topic(topic);

    // Search for a '/' after "LOG/"
    const auto level_endpos = topic.find_first_of('/', 4);
    // If a '/' was found, then topic is formatted as "LOG/LEVEL/TOPIC", else the log topic is empty
    if(level_endpos != std::string::npos) {
        log_topic_ = topic.substr(level_endpos + 1);
    }
}

std::string_view CMDP1LogMessage::getLogMessage() const {
    return get_payload().to_string_view();
}

CMDP1LogMessage CMDP1LogMessage::disassemble(zmq::multipart_t& frames) {
    // Use disassemble from base class and cast via constructor
    return {CMDP1Message::disassemble(frames)};
}

CMDP1StatMessage::CMDP1StatMessage(Header header, metrics::MetricValue metric_value)
    : CMDP1Message(
          "STAT/" + transform(metric_value.getMetric()->name(), ::toupper), std::move(header), metric_value.assemble()),
      metric_value_(std::move(metric_value)) {}

CMDP1StatMessage::CMDP1StatMessage(CMDP1Message&& message) : CMDP1Message(std::move(message)) {
    if(!isStatMessage()) [[unlikely]] {
        throw IncorrectMessageType("Not a telemetry message");
    }

    // Assign topic after prefix "STAT/"
    const auto topic = std::string(getMessageTopic().substr(5));

    try {
        metric_value_ = MetricValue::disassemble(topic, get_payload());
    } catch(const std::invalid_argument& e) {
        throw MessageDecodingError("CMDP1", e.what());
    }
}

CMDP1StatMessage CMDP1StatMessage::disassemble(zmq::multipart_t& frames) {
    // Use disassemble from base class and cast via constructor
    return {CMDP1Message::disassemble(frames)};
}

CMDP1Notification::CMDP1Notification(Header header, std::string id, Dictionary topics)
    : CMDP1Message(std::move(id), std::move(header), topics.assemble()), topics_(std::move(topics)) {}

CMDP1Notification::CMDP1Notification(CMDP1Message&& message) : CMDP1Message(std::move(message)) {
    if(!isNotification()) [[unlikely]] {
        throw IncorrectMessageType("Not a CMDP notification");
    }

    try {
        topics_ = Dictionary::disassemble(get_payload());
    } catch(const std::invalid_argument& e) {
        throw MessageDecodingError("CMDP1", e.what());
    }
}

CMDP1Notification CMDP1Notification::disassemble(zmq::multipart_t& frames) {
    // Use disassemble from base class and cast via constructor
    return {CMDP1Message::disassemble(frames)};
}
