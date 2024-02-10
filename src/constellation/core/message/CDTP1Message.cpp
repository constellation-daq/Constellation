/**
 * @file
 * @brief Implementation of CSCP1 message type
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CDTP1Message.hpp"

#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/message/Header.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std23.hpp"

using namespace constellation;
using namespace constellation::message;

CDTP1Message::CDTP1Message(CDTP1Header header, size_t frames) : header_(std::move(header)) {
    payload_frames_.reserve(frames);
}

zmq::multipart_t CDTP1Message::assemble() {
    zmq::multipart_t frames {};

    // First frame: header
    msgpack::sbuffer sbuf_header {};
    msgpack::pack(sbuf_header, header_);
    frames.addmem(sbuf_header.data(), sbuf_header.size());

    // Second frame until Nth frame: payload
    for(size_t i = 0; i < payload_frames_.size(); i++) {
        if(!payload_frames_.at(i)->empty()) {
            frames.add(zmq::message_t());
            frames.at(i + 1).swap(*payload_frames_.at(i));
        }
    }

    return frames;
}

CDTP1Message CDTP1Message::disassemble(zmq::multipart_t& frames) {
    if(frames.size() < 2) {
        // TODO(simonspa): throw
    }

    // Decode header
    const CDTP1Header header {{to_byte_ptr(frames.at(0).data()), frames.at(0).size()}};

    // Create message
    auto cdtp_message = CDTP1Message(header, frames.size() - 1);

    // Swap payload
    for(size_t i = 1; i < frames.size(); i++) {
        frames.at(i).swap(*cdtp_message.payload_frames_.at(i - 1));
    }

    return cdtp_message;
}
