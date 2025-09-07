/**
 * @file
 * @brief Implementation of CDTP2 message class
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CDTP2Message.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <msgpack.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/protocol/Protocol.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/std_future.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;

std::size_t CDTP2Message::DataRecord::countPayloadBytes() const {
    return std::transform_reduce(blocks_.begin(), blocks_.end(), 0UL, std::plus(), [](const auto& payload_buffer) {
        return payload_buffer.span().size();
    });
}

void CDTP2Message::DataRecord::msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const {
    // Array of sequence number, tags and array of byte arrays
    msgpack_packer.pack_array(3);
    msgpack_packer.pack_fix_uint64(sequence_number_);
    msgpack_packer.pack(tags_);
    msgpack_packer.pack_array(blocks_.size());
    for(const auto& block : blocks_) {
        msgpack_packer.pack_bin(block.span().size());
        msgpack_packer.pack_bin_body(to_char_ptr(block.span().data()), block.span().size());
    }
}

void CDTP2Message::DataRecord::msgpack_unpack(const msgpack::object& msgpack_object) {
    // Decode as array
    if(msgpack_object.type != msgpack::type::ARRAY) [[unlikely]] {
        throw MsgpackUnpackError("Error unpacking data", "data record is not an array");
    }
    const auto msgpack_array_raw = msgpack_object.via.array; // NOLINT(cppcoreguidelines-pro-type-union-access)
    const auto msgpack_array = std::span(msgpack_array_raw.ptr, msgpack_array_raw.size);
    // Contains three objects
    if(msgpack_array.size() != 3) [[unlikely]] {
        throw MsgpackUnpackError("Error unpacking data", "data record array has wrong size");
    }
    // Sequence number, tags and array of byte arrays
    sequence_number_ = msgpack_array[0].as<std::uint64_t>();
    tags_ = msgpack_array[1].as<Dictionary>();
    if(msgpack_array[2].type != msgpack::type::ARRAY) [[unlikely]] {
        throw MsgpackUnpackError("Error unpacking data", "data record blocks is not an array");
    }
    // Move byte arrays into payload buffers
    const auto msgpack_blocks_array_raw = msgpack_array[2].via.array; // NOLINT(cppcoreguidelines-pro-type-union-access)
    const auto msgpack_blocks_array = std::span(msgpack_blocks_array_raw.ptr, msgpack_blocks_array_raw.size);
    blocks_.reserve(msgpack_blocks_array.size());
    for(const auto& block : msgpack_blocks_array) {
        blocks_.emplace_back(block.as<std::vector<std::byte>>());
    }
}

std::size_t CDTP2Message::countPayloadBytes() const {
    return std::transform_reduce(data_records_.begin(), data_records_.end(), 0UL, std::plus(), [](const auto& data_record) {
        return data_record.countPayloadBytes();
    });
}

zmq::multipart_t CDTP2Message::assemble() const {
    msgpack::sbuffer sbuf {};
    msgpack::packer<msgpack::sbuffer> msgpack_packer {sbuf};

    // Pack header
    msgpack_packer.pack(get_protocol_identifier(CDTP2));
    msgpack_packer.pack(sender_);
    msgpack_packer.pack(std::to_underlying(type_));

    // Pack data records as array
    msgpack_packer.pack_array(data_records_.size());
    for(const auto& data_record : data_records_) {
        msgpack_packer.pack(data_record);
    }

    // Create zero-copy payload
    PayloadBuffer buffer {std::move(sbuf)};
    zmq::multipart_t msg {buffer.to_zmq_msg_release()};
    return msg;
}

CDTP2Message CDTP2Message::disassemble(zmq::multipart_t& frames) {
    try {
        if(frames.size() != 1) [[unlikely]] {
            throw MessageDecodingError("CDTP2", "Wrong number of ZeroMQ frames, exactly one frame expected");
        }
        const auto frame = frames.pop();

        // Offset since we decode multiple msgpack objects
        std::size_t offset = 0;

        // Unpack protocol
        const auto protocol_identifier = msgpack_unpack_to<std::string>(to_char_ptr(frame.data()), frame.size(), offset);
        try {
            const auto protocol_recv = get_protocol(protocol_identifier);
            if(protocol_recv != CDTP2) [[unlikely]] {
                throw UnexpectedProtocolError(protocol_recv, CDTP2);
            }
        } catch(const std::invalid_argument& error) {
            throw InvalidProtocolError(protocol_identifier);
        }

        // Unpack sender
        const auto sender = msgpack_unpack_to<std::string>(to_char_ptr(frame.data()), frame.size(), offset);

        // Unpack type
        const auto type = msgpack_unpack_to_enum<Type>(to_char_ptr(frame.data()), frame.size(), offset);

        // Unpack data records
        const auto msgpack_object = msgpack::unpack(to_char_ptr(frame.data()), frame.size(), offset);
        if(msgpack_object->type != msgpack::type::ARRAY) [[unlikely]] {
            throw MsgpackUnpackError("Error unpacking data", "data records are not in an array");
        }
        const auto msgpack_data_records_raw = msgpack_object->via.array; // NOLINT(cppcoreguidelines-pro-type-union-access)
        const auto msgpack_data_records = std::span(msgpack_data_records_raw.ptr, msgpack_data_records_raw.size);

        // Create message to append data records
        auto message = CDTP2Message(sender, type, msgpack_data_records.size());
        for(const auto& msgpack_data_record : msgpack_data_records) {
            message.addDataRecord(msgpack_data_record.as<DataRecord>());
        }

        return message;
    } catch(const MsgpackUnpackError& e) {
        throw MessageDecodingError("CDTP2", e.what());
    }
}

CDTP2BORMessage::CDTP2BORMessage(std::string sender, Dictionary user_tags, const Configuration& configuration)
    : CDTP2Message(std::move(sender), Type::BOR, 2) {
    addDataRecord({0, std::move(user_tags), 0});
    addDataRecord({1, configuration.getDictionary(Configuration::Group::ALL, Configuration::Usage::USED), 0});
}

CDTP2BORMessage::CDTP2BORMessage(CDTP2Message&& message) : CDTP2Message(std::move(message)) {
    if(getType() != Type::BOR) [[unlikely]] {
        throw IncorrectMessageType("Not a BOR message");
    }
    if(getDataRecords().size() != 2) [[unlikely]] {
        throw MessageDecodingError("CDTP2 BOR", "Wrong number of data records, exactly two data records expected");
    }
}

CDTP2EORMessage::CDTP2EORMessage(std::string sender, Dictionary user_tags, Dictionary run_metadata)
    : CDTP2Message(std::move(sender), Type::EOR, 2) {
    addDataRecord({0, std::move(user_tags), 0});
    addDataRecord({1, std::move(run_metadata), 0});
}

Configuration CDTP2BORMessage::getConfiguration() const {
    // Return as configuration and mark config keys as used
    return {getDataRecords().at(1).getTags(), true};
}

CDTP2EORMessage::CDTP2EORMessage(CDTP2Message&& message) : CDTP2Message(std::move(message)) {
    if(getType() != Type::EOR) [[unlikely]] {
        throw IncorrectMessageType("Not an EOR message");
    }
    if(getDataRecords().size() != 2) [[unlikely]] {
        throw MessageDecodingError("CDTP2 EOR", "Wrong number of data records, exactly two data records expected");
    }
}
