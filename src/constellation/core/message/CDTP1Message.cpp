/**
 * @file
 * @brief Implementation of CDTP1 message type
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
    for(auto& frame : payload_frames_) {
        if(!frame->empty()) {
            zmq::message_t new_frame {};
            new_frame.swap(*frame);
            frames.add(std::move(new_frame));
        }
    }
    // clear payload_frames_ member as payload has been swapped
    payload_frames_.clear();
    return frames;
}

CDTP1Message CDTP1Message::disassemble(zmq::multipart_t& frames) {
    if(frames.size() < 2) {
        // TODO(simonspa): throw
    }

    // Decode header
    const CDTP1Header header {{to_byte_ptr(frames.at(0).data()), frames.at(0).size()}};

    // Create message, reversing space for frames
    auto cdtp_message = CDTP1Message(header, frames.size() - 1);

    // Swap payload
    for(auto& frame : frames) {
        auto new_frame = std::make_shared<zmq::message_t>();
        new_frame->swap(frame);
        cdtp_message.addPayload(std::move(new_frame));
    }

    return cdtp_message;
}
