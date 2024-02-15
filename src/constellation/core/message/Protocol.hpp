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
    inline std::string get_protocol_identifier(Protocol protocol) {
        switch(protocol) {
        case CSCP1: return {"CSCP\01"};
        case CMDP1: return {"CMDP\01"};
        case CDTP1: return {"CDTP\01"};
        default: std::unreachable();
        }
    }

} // namespace constellation::message
