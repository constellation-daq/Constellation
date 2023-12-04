#pragma once

#include <cstdint>

namespace cnstln {
namespace CHIRP {

/**
 * Port number for a network connection
 *
 * Note that since ports are allocated dynamically, the port number should range between 49152 and 65535, as these are
 * reserved by the IANA for dynamic allocation or temporary use (see also https://en.wikipedia.org/wiki/Ephemeral_port).
 */
using Port = std::uint16_t;

/** Version of CHIRP protocol */
constexpr std::uint8_t CHIRP_VERSION = '\x01';

/** Port number of the CHIRP protocol */
constexpr Port CHIRP_PORT = 7123;

/** CHIRP Message length in bytes */
constexpr std::size_t CHIRP_MESSAGE_LENGTH = 42;

/** CHIRP message type */
enum class MessageType : std::uint8_t {
    /** A message with REQUEST type indicates that CHIRP hosts should reply with an OFFER */
    REQUEST = '\x01',

    /** A message with OFFER type indicates that service is available */
    OFFER = '\x02',

    /** A message with DEPART type indicates that a service is no longer available */
    DEPART = '\x03',
};
using enum MessageType;

/** CHIRP service identifier */
enum class ServiceIdentifier : std::uint8_t {
    /** The CONTROL service identifier indicates a CSCP (Constellation Satellite Control Protocol) service */
    CONTROL = '\x01',

    /** The HEARTBEAT service identifier indicates a CHBP (Constellation Heartbeat Broadcasting Protocol) service */
    HEARTBEAT = '\x02',

    /** The MONITORING service identifier indicates a CMDP (Constellation Monitoring Distribution Protocol) service */
    MONITORING = '\x03',

    /** The DATA service identifier indicates a CDTP (Constellation Data Transmission Protocol) service */
    DATA = '\x04',
};
using enum ServiceIdentifier;

} // namespace CHIRP
} // namespace cnstln
