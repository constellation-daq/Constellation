/**
 * @file
 * @brief Implementation of CHP1 message type
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CHP1Message.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/protocol/CHP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/protocol/Protocol.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/std_future.hpp"

using namespace constellation::message;
using namespace constellation::utils;
using namespace constellation::protocol;
using namespace std::string_view_literals;

CHP1Message CHP1Message::disassemble(zmq::multipart_t& frames) {
    if(frames.empty() || frames.size() > 2) {
        throw MessageDecodingError("CHP1", "Wrong number of frames for CHP1 message");
    }

    try {
        const auto frame = frames.pop();

        // Offset since we decode five separate msgpack objects
        std::size_t offset = 0;

        // Unpack protocol
        const auto protocol_identifier = msgpack_unpack_to<std::string>(to_char_ptr(frame.data()), frame.size(), offset);
        try {
            const auto protocol_recv = get_protocol(protocol_identifier);
            if(protocol_recv != CHP1) [[unlikely]] {
                throw UnexpectedProtocolError(protocol_recv, CHP1);
            }
        } catch(const std::invalid_argument& error) {
            throw InvalidProtocolError(protocol_identifier);
        }

        // Unpack sender
        const auto sender = msgpack_unpack_to<std::string>(to_char_ptr(frame.data()), frame.size(), offset);

        // Unpack time
        const auto time =
            msgpack_unpack_to<std::chrono::system_clock::time_point>(to_char_ptr(frame.data()), frame.size(), offset);

        // Unpack remote state
        const auto state = msgpack_unpack_to_enum<CSCP::State>(to_char_ptr(frame.data()), frame.size(), offset);

        // Unpack message flags (do not use unpack_to_enum since it is an enum flag)
        const auto flags =
            static_cast<CHP::MessageFlags>(msgpack_unpack_to<std::uint8_t>(to_char_ptr(frame.data()), frame.size(), offset));

        // Unpack time interval
        const auto interval = static_cast<std::chrono::milliseconds>(
            msgpack_unpack_to<std::uint16_t>(to_char_ptr(frame.data()), frame.size(), offset));

        // Attempt to unpack a status message
        std::optional<std::string> status {};
        if(!frames.empty()) {
            const auto status_frame = frames.pop();
            status = std::string(status_frame.to_string_view());
        }

        // Construct message
        return {sender, state, interval, flags, status, time};
    } catch(const MsgpackUnpackError& e) {
        throw MessageDecodingError("CHP1", e.what());
    }
}

zmq::multipart_t CHP1Message::assemble() {
    zmq::multipart_t frames {};

    msgpack::sbuffer sbuf {};

    // first pack protocol
    msgpack_pack(sbuf, get_protocol_identifier(CHP1));
    // then sender
    msgpack_pack(sbuf, getSender());
    // then time
    msgpack_pack(sbuf, getTime());
    // then state
    msgpack_pack(sbuf, std::to_underlying(state_));
    // then flags
    msgpack_pack(sbuf, std::to_underlying(flags_));
    // then interval
    msgpack_pack(sbuf, static_cast<uint16_t>(interval_.count()));

    frames.add(PayloadBuffer(std::move(sbuf)).to_zmq_msg_release());

    // add status to new frame if available
    if(status_.has_value()) {
        frames.add(PayloadBuffer(std::move(status_.value())).to_zmq_msg_release());
    }

    return frames;
}
