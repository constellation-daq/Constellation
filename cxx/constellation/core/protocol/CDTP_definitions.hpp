/**
 * @file
 * @brief Additional definitions for the CDTP protocol
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>

#include <magic_enum.hpp>

namespace constellation::protocol::CDTP {

    /** Possible data conditions of a run */
    enum class DataCondition : std::uint8_t {
        /** The run has concluded normally, no other information has been provided by the sender */
        GOOD,

        /** The run has concluded normally, but the data has been marked as tainted by the sender */
        TAINTED,

        /** The run has been interrupted by this sender because of an failure condition elsewhere in the constellation */
        INTERRUPTED,

        /** The run has been aborted by the sender and the EOR message has been appended by the receiver */
        ABORTED,
    };

} // namespace constellation::protocol::CDTP
