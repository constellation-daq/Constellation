#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <string>
#include <string_view>

#include "CHIRP/config.hpp"
#include "CHIRP/protocol_info.hpp"

namespace cnstln {
namespace CHIRP {

enum class MessageType : std::uint8_t {
    REQUEST = '\x01',
    OFFER = '\x02',
    DEPART = '\x03',
};
using enum MessageType;

enum class ServiceIdentifier : std::uint8_t {
    CONTROL = '\x01',
    HEARTBEAT = '\x02',
    MONITORING = '\x03',
    DATA = '\x04',
};
using enum ServiceIdentifier;

using Port = std::uint16_t;

class MD5Hash : public std::array<std::uint8_t, 16> {
public:
    constexpr MD5Hash() {}
    CHIRP_API MD5Hash(std::string_view string);
    CHIRP_API std::string to_string() const;
    CHIRP_API bool operator<(const MD5Hash& other) const;
};

class AssembledMessage : public std::array<std::uint8_t, MESSAGE_LENGTH> {
public:
    constexpr AssembledMessage() {}
    CHIRP_API AssembledMessage(const std::vector<std::uint8_t>& assembled_message);
};

class Message {
public:
    CHIRP_API Message(MessageType type, MD5Hash group_hash, MD5Hash host_hash, ServiceIdentifier service, Port port);
    CHIRP_API Message(MessageType type, std::string_view group, std::string_view host, ServiceIdentifier service, Port port);
    CHIRP_API Message(const AssembledMessage& assembled_message);

    constexpr MessageType GetType() const { return type_; }
    constexpr MD5Hash GetGroupHash() const { return group_hash_; }
    constexpr MD5Hash GetHostHash() const { return host_hash_; }
    constexpr ServiceIdentifier GetServiceIdentifier() const { return service_; }
    constexpr Port GetPort() const { return port_; }

    CHIRP_API AssembledMessage Assemble() const;

private:
    MessageType type_;
    MD5Hash group_hash_;
    MD5Hash host_hash_;
    ServiceIdentifier service_;
    Port port_;
};

} // namespace CHIRP
} // namespace cnstln
