/**
 * @file
 * @brief Implementation of CSCP1 message type
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CSCP1Message.hpp"

#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/message/CSCP1Header.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std23.hpp"

using namespace constellation::message;
using namespace constellation::utils;

CSCP1Message::CSCP1Message(CSCP1Header header, std::pair<CSCP1Type, std::string> verb)
    : header_(std::move(header)), verb_(std::move(verb)) {}

zmq::multipart_t CSCP1Message::assemble() {
    zmq::multipart_t frames {};

    // First frame: header
    msgpack::sbuffer sbuf_header {};
    msgpack::pack(sbuf_header, header_);
    frames.addmem(sbuf_header.data(), sbuf_header.size());

    // Second frame: body
    msgpack::sbuffer sbuf_body {};
    msgpack::packer packer_body {&sbuf_body};
    packer_body.pack(std::to_underlying(verb_.first));
    packer_body.pack(verb_.second);
    frames.addmem(sbuf_body.data(), sbuf_body.size());

    // Third frame: swap payload if available
    if(payload_ && !payload_->empty()) {
        zmq::message_t payload_frame {};
        payload_frame.swap(*payload_);
        frames.add(std::move(payload_frame));
    }

    return frames;
}

CSCP1Message CSCP1Message::disassemble(zmq::multipart_t& frames) {
    if(frames.size() < 2 || frames.size() > 3) {
        // TODO(stephan.lachnit): throw
    }

    // Decode header
    const auto header = CSCP1Header::disassemble({to_byte_ptr(frames.at(0).data()), frames.at(0).size()});

    // Decode body
    std::size_t offset = 0;
    const auto msgpack_type = msgpack::unpack(to_char_ptr(frames.at(1).data()), frames.at(1).size(), offset);
    const auto type = static_cast<CSCP1Type>(msgpack_type->as<std::uint8_t>());
    const auto msgpack_string = msgpack::unpack(to_char_ptr(frames.at(1).data()), frames.at(1).size(), offset);
    const auto string = msgpack_string->as<std::string>();

    // Create message
    auto cscp1_message = CSCP1Message(header, {type, string});

    // Swap payload if available
    if(frames.size() == 3) {
        cscp1_message.payload_ = std::make_shared<zmq::message_t>();
        frames.at(2).swap(*cscp1_message.payload_);
    }

    return cscp1_message;
}
