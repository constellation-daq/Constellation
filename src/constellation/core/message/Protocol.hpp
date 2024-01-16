/**
 * @file
 * @brief Message Protocol Enum
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

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

} // namespace constellation::message
