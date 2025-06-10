/**
 * @file
 * @brief CHIRP protocol definitions
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "constellation/core/networking/Port.hpp"

namespace constellation::protocol::CHIRP {

    /** Protocol identifier for CHIRP */
    constexpr std::string_view IDENTIFIER = "CHIRP";

    /** Version of CHIRP protocol */
    constexpr std::uint8_t VERSION = '\x01';

    /** Multicast address of the CHIRP protocol */
    constexpr std::array<unsigned char, 4> MULTICAST_ADDRESS = {239, 192, 7, 123};

    /** Port number of the CHIRP protocol */
    constexpr networking::Port PORT = 7123;

    /** CHIRP Message length in bytes */
    constexpr std::size_t MESSAGE_LENGTH = 42;

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

        /** The HEARTBEAT service identifier indicates a CHP (Constellation Heartbeat Protocol) service */
        HEARTBEAT = '\x02',

        /** The MONITORING service identifier indicates a CMDP (Constellation Monitoring Distribution Protocol) service */
        MONITORING = '\x03',

        /** The DATA service identifier indicates a CDTP (Constellation Data Transmission Protocol) service */
        DATA = '\x04',
    };
    using enum ServiceIdentifier;

} // namespace constellation::protocol::CHIRP
