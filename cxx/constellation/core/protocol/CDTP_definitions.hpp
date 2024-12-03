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

namespace constellation::protocol::CDTP {

    /** Possible run conditions of a run */
    enum class RunCondition : std::uint8_t {
        /** The run has concluded normally, no other information has been provided by the sender */
        GOOD = 0x00,

        /** The run has concluded normally, but the data has been marked as tainted by the sender */
        TAINTED = 0x01,

        /** The run has concluded normally, but the receiver has noticed missing messages in the sequence */
        INCOMPLETE = 0x02,

        /** The run has been interrupted by this sender because of a failure condition elsewhere in the constellation */
        INTERRUPTED = 0xFE,

        /** The run has been aborted by the sender and the EOR message has been appended by the receiver */
        ABORTED = 0xFF,
    };

} // namespace constellation::protocol::CDTP

// Run condition enum exceeds default enum value limits of magic_enum (-128, 127)
#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#include <magic_enum.hpp>
#endif
namespace magic_enum::customize {
    template <> struct enum_range<constellation::protocol::CDTP::RunCondition> {
        static constexpr int min = 0;
        static constexpr int max = 255;
    };
} // namespace magic_enum::customize
