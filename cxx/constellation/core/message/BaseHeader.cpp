/**
 * @file
 * @brief Implementation of Message Header
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "BaseHeader.hpp"

#include <chrono>
#include <cstddef>
#include <ios>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>

#include <msgpack.hpp>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/protocol/Protocol.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;

BaseHeader BaseHeader::disassemble(Protocol protocol, std::span<const std::byte> data) {
    try {
        // Offset since we decode four separate msgpack objects
        std::size_t offset = 0;

        // Unpack protocol
        const auto protocol_identifier = msgpack_unpack_to<std::string>(to_char_ptr(data.data()), data.size(), offset);
        try {
            const auto protocol_recv = get_protocol(protocol_identifier);
            if(protocol_recv != protocol) [[unlikely]] {
                throw UnexpectedProtocolError(protocol_recv, protocol);
            }
        } catch(const std::invalid_argument& error) {
            throw InvalidProtocolError(protocol_identifier);
        }

        // Unpack sender
        const auto sender = msgpack_unpack_to<std::string>(to_char_ptr(data.data()), data.size_bytes(), offset);

        // Unpack time
        const auto time =
            msgpack_unpack_to<std::chrono::system_clock::time_point>(to_char_ptr(data.data()), data.size_bytes(), offset);

        // Unpack tags
        const auto tags = msgpack_unpack_to<Dictionary>(to_char_ptr(data.data()), data.size_bytes(), offset);

        // Construct header
        return {protocol, sender, time, tags};
    } catch(const MsgpackUnpackError& e) {
        throw MessageDecodingError(get_readable_protocol(protocol), e.what());
    }
}

void BaseHeader::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    // first pack version
    msgpack_packer.pack(get_protocol_identifier(protocol_));
    // then sender
    msgpack_packer.pack(sender_);
    // then time
    msgpack_packer.pack(time_);
    // then tags
    msgpack_packer.pack(tags_);
}

std::string BaseHeader::to_string() const {
    std::ostringstream out {};
    std::boolalpha(out);
    out << "Header: " << get_readable_protocol(protocol_) << '\n' //
        << "Sender: " << sender_ << '\n'                          //
        << "Time:   " << utils::to_string(time_) << '\n'          //
        << "Tags:" << tags_.to_string();

    return out.str();
}
