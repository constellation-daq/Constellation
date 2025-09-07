/**
 * @file
 * @brief Implementation of CSCP1 message type
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CSCP1Message.hpp"

#include <cstddef>
#include <string>
#include <utility>

#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/std_future.hpp"

using namespace constellation::message;
using namespace constellation::utils;

CSCP1Message::CSCP1Message(CSCP1Message::Header header, std::pair<Type, std::string> verb)
    : header_(std::move(header)), verb_(std::move(verb)) {}

zmq::multipart_t CSCP1Message::assemble(bool keep_payload) {
    zmq::multipart_t frames {};

    // First frame: header
    msgpack::sbuffer sbuf_header {};
    msgpack_pack(sbuf_header, header_);
    frames.add(PayloadBuffer(std::move(sbuf_header)).to_zmq_msg_release());

    // Second frame: body
    msgpack::sbuffer sbuf_body {};
    msgpack_pack(sbuf_body, std::to_underlying(verb_.first));
    msgpack_pack(sbuf_body, verb_.second);
    frames.add(PayloadBuffer(std::move(sbuf_body)).to_zmq_msg_release());

    // Third frame: payload
    if(hasPayload()) {
        frames.add(keep_payload ? payload_.to_zmq_msg_copy() : payload_.to_zmq_msg_release());
    }

    return frames;
}

CSCP1Message CSCP1Message::disassemble(zmq::multipart_t& frames) {
    if(frames.size() < 2 || frames.size() > 3) {
        throw MessageDecodingError("CSCP1", "Incorrect number of message frames");
    }

    // Decode header
    const auto header_frame = frames.pop();
    const auto header = Header::disassemble({to_byte_ptr(header_frame.data()), header_frame.size()});

    try {
        // Decode body
        const auto body_frame = frames.pop();
        std::size_t offset = 0;
        const auto type = msgpack_unpack_to_enum<Type>(to_char_ptr(body_frame.data()), body_frame.size(), offset);
        const auto string = msgpack_unpack_to<std::string>(to_char_ptr(body_frame.data()), body_frame.size(), offset);

        // Create message
        auto cscp1_message = CSCP1Message(header, {type, string});

        // Move payload if available
        if(!frames.empty()) {
            cscp1_message.payload_ = {frames.pop()};
        }

        return cscp1_message;
    } catch(const MsgpackUnpackError& e) {
        throw MessageDecodingError("CSCP1", e.what());
    }
}
