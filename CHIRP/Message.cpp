#include "Message.hpp"

#include "md5.h"

using namespace CHIRP;

MD5Hash::MD5Hash(std::string_view string) {
    auto hasher = Chocobo1::MD5();
    hasher.addData(string.data(), string.length());
    hasher.finalize();
    auto hash_copy = hasher.toArray();
    this->swap(hash_copy);
}

std::string MD5Hash::toString() const {
    std::string ret;
    ret.resize(2 * this->size());
    for (std::uint8_t n = 0; n < this->size(); ++n) {
        const auto hash_upper = (this->at(n) & 0xF0) >> 4;
        ret.at(2*n) = (hash_upper < 10) ? static_cast<char>(hash_upper + '0') : static_cast<char>(hash_upper - 10 + 'a');
        const auto hash_lower = this->at(n) & 0x0F;
        ret.at(2*n+1) = (hash_lower < 10) ? static_cast<char>(hash_lower + '0') : static_cast<char>(hash_lower - 10 + 'a');
    }
    return ret;
}

Message::Message(std::string_view group, std::string_view name, MessageType type, ServiceIdentifier service, Port port)
  : group_hash_(MD5Hash(group)), name_hash_(MD5Hash(name)),
    type_(type), service_(service), port_(port) {}

Message::Message(MD5Hash group_hash, MD5Hash name_hash, MessageType type, ServiceIdentifier service, Port port)
  : group_hash_(std::move(group_hash)), name_hash_(std::move(name_hash)),
    type_(type), service_(service), port_(port) {}

Message::Message(std::vector<char>& assembled_message) {
    // TODO: check size
    // TODO: check the first 6 bytes and enum ranges
    type_ = static_cast<MessageType>(assembled_message[6]);
    for (std::uint8_t n = 0; n < 16; ++n) {
        group_hash_[n] = static_cast<std::uint8_t>(assembled_message[7+n]);
    }
    for (std::uint8_t n = 0; n < 16; ++n) {
        name_hash_[n] = static_cast<std::uint8_t>(assembled_message[23+n]);
    }
    service_ = static_cast<ServiceIdentifier>(assembled_message[39]);
    port_ = static_cast<std::uint8_t>(assembled_message[40]) + (static_cast<std::uint16_t>(assembled_message[41]) << 8);
}

Message::Message(std::array<std::uint8_t, MESSAGE_LENGTH>& assembled_message) {
    // TODO: check the first 6 bytes and enum ranges
    type_ = static_cast<MessageType>(assembled_message[6]);
    for (std::uint8_t n = 0; n < 16; ++n) {
        group_hash_[n] = assembled_message[7+n];
    }
    for (std::uint8_t n = 0; n < 16; ++n) {
        name_hash_[n] = assembled_message[23+n];
    }
    service_ = static_cast<ServiceIdentifier>(assembled_message[39]);
    port_ = assembled_message[40] + (static_cast<std::uint16_t>(assembled_message[41]) << 8);
}

std::array<std::uint8_t, 42> Message::Assemble() const {
    std::array<std::uint8_t, 42> out;
    out[0] = 'C';
    out[1] = 'H';
    out[2] = 'I';
    out[3] = 'R';
    out[4] = 'P';
    out[5] = CHIRP_VERSION;
    out[6] = static_cast<std::uint8_t>(type_);
    for (std::uint8_t n = 0; n < 16; ++n) {
        out[7+n] = group_hash_[n];
    }
    for (std::uint8_t n = 0; n < 16; ++n) {
        out[23+n] = name_hash_[n];
    }
    out[39] = static_cast<std::uint8_t>(service_);
    out[40] = static_cast<std::uint8_t>(port_ & 0x00FF);
    out[41] = static_cast<std::uint8_t>((port_ >> 8) & 0x00FF);
    return out;
}
