/**
 * @file
 * @brief Message Protocol Enum
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "constellation/core/utils/std23.hpp"

namespace constellation::message {

    /** Protocol Enum (excluding CHIRP) */
    enum class Protocol {
        /** Constellation Satellite Control Protocol v1 */
        CSCP1,
        /** Constellation Monitoring Distribution Protocol v1 */
        CMDP1,
        /** Constellation Data Transmission Protocol v1 */
        CDTP1,
    };
    using enum Protocol;

    /**
     * Get protocol identifier string for CSCP, CMDP and CDTP protocols
     *
     * @param protocol Protocol
     * @return Protocol identifier string in message header
     */
    constexpr std::string_view get_protocol_identifier(Protocol protocol) {
        switch(protocol) {
        case CSCP1: return {"CSCP\x01"};
        case CMDP1: return {"CMDP\x01"};
        case CDTP1: return {"CDTP\x01"};
        default: std::unreachable();
        }
    }

    /**
     * Get human-readable protocol identifier string for CSCP, CMDP and CDTP protocols
     *
     * @param protocol Protocol
     * @return Protocol identifier string with byte version replaced to human-readable version
     */
    inline std::string get_readable_protocol(std::string_view protocol_identifier) {
        std::string out {protocol_identifier.data(), protocol_identifier.size() - 1};
        out += std::to_string(protocol_identifier.back());
        return out;
    }

    inline std::string get_readable_protocol(Protocol protocol) {
        auto protocol_identifier = get_protocol_identifier(protocol);
        return get_readable_protocol(protocol_identifier);
    }

} // namespace constellation::message
