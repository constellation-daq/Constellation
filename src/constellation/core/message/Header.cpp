/**
 * @file
 * @brief Implementation of Message Header
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Header.hpp"

#include "constellation/core/message/Protocol.hpp"
#include "constellation/core/std23.hpp"
#include "constellation/core/utils/stdbyte_casts.hpp"

#include <sstream>
#include <string_view>

using namespace constellation::message;
using namespace std::literals::string_view_literals;

/**
 * Get protocol identifier string for CSCP, CMDP and CDTP protocols
 *
 * @param protocol Protocol
 * @return Protocol identifier string in message header
 */
constexpr std::string_view get_protocol_identifier(Protocol protocol) {
    switch(protocol) {
    case CSCP1: return "CSCP\01"sv;
    case CMDP1: return "CMDP\01"sv;
    case CDTP1: return "CDTP\01"sv;
    default: std::unreachable();
    }
}

template <Protocol P>
Header<P>::Header(std::string_view sender, std::chrono::system_clock::time_point time) : sender_(sender), time_(time) {}

template <Protocol P> Header<P>::Header(std::string_view sender) : Header(sender, std::chrono::system_clock::now()) {}

template <Protocol P> Header<P>::Header(std::span<std::byte> data) {
    // Offset since we decode four separate msgpack objects
    std::size_t offset = 0;

    // Unpack protocol
    const auto msgpack_protocol_identifier = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
    const auto protocol_identifier = msgpack_protocol_identifier->as<std::string>();
    if(protocol_identifier != get_protocol_identifier(P)) {
        // TODO(stephan.lachnit): throw
    }

    // Unpack sender
    const auto msgpack_sender = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
    sender_ = msgpack_sender->as<std::string>();

    // Unpack time
    const auto msgpack_time = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
    time_ = msgpack_time->as<std::chrono::system_clock::time_point>();

    // Unpack tags
    const auto msgpack_tags = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
    tags_ = msgpack_tags->as<dictionary_t>();
}

template <Protocol P> msgpack::sbuffer Header<P>::assemble() const {
    msgpack::sbuffer sbuf {};
    msgpack::packer<msgpack::sbuffer> packer {&sbuf};

    // first pack version
    packer.pack(get_protocol_identifier(P));
    // then sender
    packer.pack(sender_);
    // then time
    packer.pack(time_);
    // then tags
    packer.pack(tags_);

    // content can be accessed via .data() and .size()
    return sbuf;
}

template <Protocol P> std::string Header<P>::to_string() const {
    std::ostringstream out {};
    out << "Header: " << get_protocol_identifier(P) << "\n"
        << "Sender: " << sender_ << "\n"
        << "Time:   " << time_ << "\n"
        << "Tags:\n";
    for(const auto& entry : tags_) {
        // TODO(stephan.lachnit): second entry evaluation should be in a try / catch block
        out << " " << entry.first << ": ";
        std::visit([&](auto&& arg) { out << arg; }, entry.second);
        out << "\n";
    }
    return out.str();
}

// Export symbols in shared library
namespace constellation::message {
    template class Header<CSCP1>;
    template class Header<CMDP1>;
    template class Header<CDTP1>;
} // namespace constellation::message
