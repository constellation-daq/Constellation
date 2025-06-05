/**
 * @file
 * @brief Additional definitions for the CHP protocol
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace constellation::protocol::CHP {

    /** Default lives for a remote on detection/replenishment */
    static constexpr std::uint8_t Lives = 3;

    /**
     * @brief Method to calculate the heartbeat interval based on the number of subscriber satellites and a maximum interval
     * Using a load factor of 5 to scale down number of messages
     *
     * @param subscribers Current number of subscriber satellites
     * @param max Maximum allowed interval between heartbeats
     *
     * @return New heartbeat interval
     */
    constexpr std::chrono::milliseconds calculate_interval(std::size_t subscribers, std::chrono::milliseconds max) {
        using namespace std::chrono_literals;
        return std::min(max,
                        std::max(500ms,
                                 std::chrono::duration_cast<std::chrono::milliseconds>(
                                     500ms * std::sqrt(static_cast<double>(subscribers)) * 5.)));
    }

} // namespace constellation::protocol::CHP
