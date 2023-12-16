/**
 * @file
 * @brief Implementation of CMDP Message Header
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MessageHeader.hpp"

#include <iostream>

MessageHeader::MessageHeader(void* data, std::size_t size) {
    // Offset since we decode four separate msgpack objects
    std::size_t offset = 0;

    // Unpack protocol
    const auto msgpack_protocol = msgpack::unpack(static_cast<char*>(data), size, offset);
    const auto protocol = msgpack_protocol->as<std::string>();
    if(protocol != CMDP1_PROTOCOL) {
        // Not CMDP version 1 message
        // TODO: throw
    }

    // Unpack sender
    const auto msgpack_sender = msgpack::unpack(static_cast<char*>(data), size, offset);
    sender_ = msgpack_sender->as<std::string>();

    // Unpack time
    const auto msgpack_time = msgpack::unpack(static_cast<char*>(data), size, offset);
    time_ = msgpack_time->as<std::chrono::system_clock::time_point>();

    // Unpack tags
    const auto msgpack_tags = msgpack::unpack(static_cast<char*>(data), size, offset);
    tags_ = msgpack_tags->as<std::map<std::string, msgpack::type::variant>>();
}

msgpack::sbuffer MessageHeader::assemble() const {
    msgpack::sbuffer sbuf {};
    msgpack::packer<msgpack::sbuffer> packer {&sbuf};

    // first pack version
    packer.pack(CMDP1_PROTOCOL);
    // then sender
    packer.pack(sender_);
    // then time
    packer.pack(time_);
    // then tags
    packer.pack(tags_);

    // content can be accessed via .data() and .size()
    return sbuf;
}

// TODO: we probably want this as a stream
void MessageHeader::print() const {
    std::cout << "Header: CMDP1\n"
              << "Sender: " << sender_ << "\n"
              << "Time:   " << time_ << "\n"
              << "Tags:\n";
    for(const auto& entry : tags_) {
        // TODO: second part should probably be in a try / catch block?
        std::cout << " " << entry.first << ": " << entry.second.as_string() << "\n";
    }
    std::cout << std::flush;
}
