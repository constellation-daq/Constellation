/**
 * @file
 * @brief Implementation of the CHIRP message
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Message.hpp"

#include <cstring>
#include <utility>

#include "constellation/chirp/exceptions.hpp"
#include "constellation/chirp/external/md5.h"
#include "constellation/core/std23.hpp"

using namespace constellation::chirp;

MD5Hash::MD5Hash(std::string_view string) : array() {
    auto hasher = Chocobo1::MD5();
    hasher.addData(string.data(), string.length());
    hasher.finalize();
    // Swap output of hash with this to avoid copy
    auto hash_copy = hasher.toArray();
    this->swap(hash_copy);
}

std::string MD5Hash::to_string() const {
    std::string ret {};
    // Resize string to twice the hash length as two character needed per byte
    ret.resize(2 * this->size());
    for(std::size_t n = 0; n < this->size(); ++n) {
        // First character of byte hex representation
        const auto hash_upper = (this->at(n) & 0xF0U) >> 4U;
        ret.at(2 * n) = static_cast<char>(hash_upper < 10 ? hash_upper + '0' : hash_upper - 10 + 'a');
        // Second character of byte hex representation
        const auto hash_lower = this->at(n) & 0x0FU;
        ret.at(2 * n + 1) = static_cast<char>(hash_lower < 10 ? hash_lower + '0' : hash_lower - 10 + 'a');
    }
    return ret;
}

bool MD5Hash::operator<(const MD5Hash& other) const {
    for(std::size_t n = 0; n < this->size(); ++n) {
        if(this->at(n) < other.at(n)) {
            return true;
        }
    }
    return false;
}

Message::Message(MessageType type, MD5Hash group_id, MD5Hash host_id, ServiceIdentifier service_id, Port port)
    : type_(type), group_id_(group_id), host_id_(host_id), service_id_(service_id), port_(port) {}

Message::Message(MessageType type, std::string_view group, std::string_view host, ServiceIdentifier service_id, Port port)
    : Message(type, MD5Hash(group), MD5Hash(host), service_id, port) {}

Message::Message(std::span<const std::uint8_t> assembled_message) : group_id_(), host_id_() {
    // Check size
    if(assembled_message.size() != CHIRP_MESSAGE_LENGTH) {
        throw DecodeError("Message length is not " + std::to_string(CHIRP_MESSAGE_LENGTH) + " bytes");
    }
    // Header
    if(std::memcmp(assembled_message.data(), CHIRP_VERSION.data(), 6) != 0) {
        throw DecodeError("Not a CHIRP v1 broadcast");
    }
    // Message Type
    if(assembled_message[6] < std::to_underlying(MessageType::REQUEST) ||
       assembled_message[6] > std::to_underlying(MessageType::DEPART)) {
        throw DecodeError("Message Type invalid");
    }
    type_ = static_cast<MessageType>(assembled_message[6]);
    // Group ID
    for(std::uint8_t n = 0; n < 16; ++n) {
        group_id_.at(n) = assembled_message[7 + n];
    }
    // Host ID
    for(std::uint8_t n = 0; n < 16; ++n) {
        host_id_.at(n) = assembled_message[23 + n];
    }
    // Service Identifier
    if(assembled_message[39] < std::to_underlying(ServiceIdentifier::CONTROL) ||
       assembled_message[39] > std::to_underlying(ServiceIdentifier::DATA)) {
        throw DecodeError("Service Identifier invalid");
    }
    service_id_ = static_cast<ServiceIdentifier>(assembled_message[39]);
    // Port
    port_ = assembled_message[40] + static_cast<std::uint16_t>(assembled_message[41] << 8U);
}

AssembledMessage Message::Assemble() const {
    AssembledMessage ret {};

    // Header
    std::memcpy(ret.data(), CHIRP_VERSION.data(), 6);
    // Message Type
    ret[6] = std::to_underlying(type_);
    // Group Hash
    for(std::uint8_t n = 0; n < 16; ++n) {
        ret.at(7 + n) = group_id_.at(n);
    }
    // Host Hash
    for(std::uint8_t n = 0; n < 16; ++n) {
        ret.at(23 + n) = host_id_.at(n);
    }
    // Service Identifier
    ret[39] = std::to_underlying(service_id_);
    // Port
    ret[40] = static_cast<std::uint8_t>(port_ & 0x00FFU);
    ret[41] = static_cast<std::uint8_t>(static_cast<unsigned int>(port_ >> 8U) & 0x00FFU);

    return ret;
}
