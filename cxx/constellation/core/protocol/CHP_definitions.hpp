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
#include <cstdint>

namespace constellation::protocol::CHP {

    /** Default lives for a remote on detection/replenishment */
    static constexpr std::uint8_t Lives = 3;

    /**
     * @brief Method to calculate current heartbeat interval based on the number of satellites and a maximum interval
     *
     * @param sats Current number of subscriber satellites
     * @param max Maximum allowed interval between heartbeats
     *
     * @return New heartbeat interval
     */
    constexpr std::chrono::milliseconds calculate_interval(std::size_t sats, std::chrono::milliseconds max) {
        using namespace std::chrono_literals;
        return std::min(max, std::chrono::duration_cast<std::chrono::milliseconds>(max * std::pow(0.01 * sats, 2) + 500ms));
    }

} // namespace constellation::protocol::CHP
