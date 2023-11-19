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

/** MD5 hash stored as array with 16 bytes */
class MD5Hash : public std::array<std::uint8_t, 16> {
public:
    constexpr MD5Hash() {}

    /**
     * Construct MD5 hash from a string
     *
     * @param string String from which to create the MD5 hash
     */
    CHIRP_API MD5Hash(std::string_view string);

    /**
     * Convert MD5 hash to an human readable string
     *
     * @returns String containing a lowercase hex representation of the MD5 hash
     */
    CHIRP_API std::string to_string() const;

    CHIRP_API bool operator<(const MD5Hash& other) const;
};

/** CHIRP message assembled to array of bytes */
class AssembledMessage : public std::array<std::uint8_t, CHIRP_MESSAGE_LENGTH> {
public:
    constexpr AssembledMessage() {}

    /**
     * Construct message from byte array with arbitrary length
     *
     * @param byte_array Reference to byte array from which the message will be copied
     * @throws :cpp:class:`DecodeError` When the byte array has a different length than :cpp:var:`CHIRP_MESSAGE_LENGTH`
     */
    CHIRP_API AssembledMessage(const std::vector<std::uint8_t>& byte_array);
};

/** CHIRP message */
class Message {
public:
    /**
     * Construct new CHIRP message
     *
     * @param type
     * @param group_id
     * @param host_id
     * @param service_id
     * @param port
     */
    CHIRP_API Message(MessageType type, MD5Hash group_id, MD5Hash host_id, ServiceIdentifier service_id, Port port);

    /**
     * Construct new CHIRP message using strings for group and host ID
     *
     * @param type
     * @param group Name of the group (converted to group ID using :cpp:class:`MD5Hash`)
     * @param host Name of the host (converted to host ID using :cpp:class:`MD5Hash`)
     * @param service_id
     * @param port
     */
    CHIRP_API Message(MessageType type, std::string_view group, std::string_view host, ServiceIdentifier service_id, Port port);

    /**
     * Constructor for a CHIRP message from an assembled message
     *
     * @param assembled_message Reference to assembled message
     * @throws :cpp:class:`DecodeError` If the message header does not match the CHIRP specification, or if the message has
     *         an unknown :cpp:enum:`MessageType` or :cpp:enum:`ServiceIdentifier`
     */
    CHIRP_API Message(const AssembledMessage& assembled_message);

    /** Return the message type */
    constexpr MessageType GetType() const { return type_; }

    /** Return the group ID of the message */
    constexpr MD5Hash GetGroupID() const { return group_id_; }

    /** Return the host ID of the message */
    constexpr MD5Hash GetHostID() const { return host_id_; }

    /** Return the service identifier of the message */
    constexpr ServiceIdentifier GetServiceIdentifier() const { return service_id_; }

    /** Return the service port of the message */
    constexpr Port GetPort() const { return port_; }

    /** Assemble message to byte array */
    CHIRP_API AssembledMessage Assemble() const;

private:
    MessageType type_;
    MD5Hash group_id_;
    MD5Hash host_id_;
    ServiceIdentifier service_id_;
    Port port_;
};

} // namespace CHIRP
} // namespace cnstln
