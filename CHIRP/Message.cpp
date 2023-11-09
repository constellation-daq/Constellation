#include "Message.hpp"

#include <algorithm>
#include <utility>

#include "CHIRP/exceptions.hpp"
#include "CHIRP/external/md5.h"

using namespace cnstln::CHIRP;

MD5Hash::MD5Hash(std::string_view string) {
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
    for (std::uint8_t n = 0; n < this->size(); ++n) {
        // First character of byte hex representation
        const auto hash_upper = static_cast<char>((this->at(n) & 0xF0) >> 4);
        ret.at(2*n) = (hash_upper < 10) ? (hash_upper + '0') : (hash_upper - 10 + 'a');
        // Second character of byte hex representation
        const auto hash_lower = static_cast<char>(this->at(n) & 0x0F);
        ret.at(2*n+1) = (hash_lower < 10) ? (hash_lower + '0') : (hash_lower - 10 + 'a');
    }
    return ret;
}

bool MD5Hash::operator<(const MD5Hash& other) const {
    for (std::uint8_t n = 0; n < this->size(); ++n) {
        if (this->at(n) < other[n]) {
            return true;
        }
    }
    return false;
}

AssembledMessage::AssembledMessage(const std::vector<std::uint8_t>& assembled_message) {
    if (assembled_message.size() != MESSAGE_LENGTH) {
        throw DecodeError("Message length is not " + std::to_string(MESSAGE_LENGTH) + " bytes");
    }
    std::copy_n(assembled_message.cbegin(), MESSAGE_LENGTH, this->begin());
}

Message::Message(MessageType type, MD5Hash group_hash, MD5Hash host_hash, ServiceIdentifier service, Port port)
  : type_(type), group_hash_(std::move(group_hash)), host_hash_(std::move(host_hash)), service_(service), port_(port) {}

Message::Message(MessageType type, std::string_view group, std::string_view host, ServiceIdentifier service, Port port)
  : Message(type, MD5Hash(group), MD5Hash(host), service, port) {}

Message::Message(const AssembledMessage& assembled_message) {
    // Header
    if (assembled_message[0] != 'C' ||
        assembled_message[1] != 'H' ||
        assembled_message[2] != 'I' ||
        assembled_message[3] != 'R' ||
        assembled_message[4] != 'P' ||
        assembled_message[5] != CHIRP_VERSION) {
        throw DecodeError("Not a CHIRP v1 broadcast");
    }
    // Message Type
    if (assembled_message[6] < std::to_underlying(MessageType::REQUEST) ||
        assembled_message[6] > std::to_underlying(MessageType::DEPART)) {
        throw DecodeError("Message Type invalid");
    }
    type_ = static_cast<MessageType>(assembled_message[6]);
    // Group Hash
    for (std::uint8_t n = 0; n < 16; ++n) {
        group_hash_[n] = assembled_message[7+n];
    }
    // Host Hash
    for (std::uint8_t n = 0; n < 16; ++n) {
        host_hash_[n] = assembled_message[23+n];
    }
    // Service Identifier
    if (assembled_message[39] < std::to_underlying(ServiceIdentifier::CONTROL) ||
        assembled_message[39] > std::to_underlying(ServiceIdentifier::DATA)) {
        throw DecodeError("Service Identifier invalid");
    }
    service_ = static_cast<ServiceIdentifier>(assembled_message[39]);
    // Port
    port_ = assembled_message[40] + (static_cast<std::uint16_t>(assembled_message[41]) << 8);
}

AssembledMessage Message::Assemble() const {
    AssembledMessage ret {};

    // Header
    ret[0] = 'C';
    ret[1] = 'H';
    ret[2] = 'I';
    ret[3] = 'R';
    ret[4] = 'P';
    ret[5] = CHIRP_VERSION;
    // Message Type
    ret[6] = std::to_underlying(type_);
    // Group Hash
    for (std::uint8_t n = 0; n < 16; ++n) {
        ret[7+n] = group_hash_[n];
    }
    // Host Hash
    for (std::uint8_t n = 0; n < 16; ++n) {
        ret[23+n] = host_hash_[n];
    }
    // Service Identifier
    ret[39] = std::to_underlying(service_);
    // Port
    ret[40] = static_cast<std::uint8_t>(port_ & 0x00FF);
    ret[41] = static_cast<std::uint8_t>((port_ >> 8) & 0x00FF);

    return ret;
}
