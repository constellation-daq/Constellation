/**
 * @file
 * @brief Implementation of Message Header
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Header.hpp"

#include <ios>
#include <sstream>
#include <string>
#include <string_view>

#include <magic_enum.hpp>
#include <msgpack.hpp>

#include "constellation/core/message/Protocol.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std23.hpp"

using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::string_view_literals;

template <Protocol P> Header<P> Header<P>::disassemble(std::span<const std::byte> data) {
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
    const auto sender = msgpack_sender->as<std::string>();

    // Unpack time
    const auto msgpack_time = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
    const auto time = msgpack_time->as<std::chrono::system_clock::time_point>();

    // Unpack tags
    const auto msgpack_tags = msgpack::unpack(to_char_ptr(data.data()), data.size_bytes(), offset);
    const auto tags = msgpack_tags->as<Dictionary>();

    // Construct header
    auto header = Header<P>(sender, time);
    header.tags_ = tags;

    return header;
}

template <Protocol P> void Header<P>::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    // first pack version
    msgpack_packer.pack(get_protocol_identifier(P));
    // then sender
    msgpack_packer.pack(sender_);
    // then time
    msgpack_packer.pack(time_);
    // then tags
    msgpack_packer.pack(tags_);
}

template <Protocol P> std::string Header<P>::to_string() const {
    // Make protocol identifier version human readable
    auto protocol = get_protocol_identifier(P);
    protocol.back() = static_cast<char>(protocol.back() + '0');
    // Stream message
    std::ostringstream out {};
    std::boolalpha(out);
    out << "Header: "sv << protocol << '\n' //
        << "Sender: "sv << sender_ << '\n'  //
        << "Time:   "sv << time_ << '\n'    //
        << "Tags:"sv;
    for(const auto& entry : tags_) {
        out << "\n "sv << entry.first << ": "sv;
        std::visit([&](auto&& arg) { out << arg; }, entry.second);
    }
    return out.str();
}

// Export symbols in shared library
namespace constellation::message {
    template class Header<CSCP1>;
    template class Header<CMDP1>;
    template class Header<CDTP1>;
} // namespace constellation::message
