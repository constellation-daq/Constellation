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

    /** Possible CHP message flags */
    enum MessageFlags : std::uint8_t {
        NONE = 0x00,

        /* Indicate a extrasystole message */
        IS_EXTRASYSTOLE = 0x01,

        /* Indicate whether the current state has been reached autonomously or by CSCP command */
        IS_AUTONOMOUS = 0x02,
    };

    /** Minimal interval between heartbeat messages */
    static constexpr std::chrono::milliseconds MinimumInterval = std::chrono::milliseconds(500);

    /** Default maximum interval between heartbeat messages */
    static constexpr std::chrono::milliseconds MaximumInterval = std::chrono::milliseconds(300000);

    /** Load factor to scale CHP subscriber weight in interval scaling */
    static constexpr double LoadFactor = 3.;

    /**
     * @brief Method to calculate the heartbeat interval based on the number of subscriber satellites and a maximum interval
     * Using a load factor to scale down number of messages
     *
     * @param subscribers Current number of subscriber satellites
     * @param max Maximum allowed interval between heartbeats
     *
     * @return New heartbeat interval
     */
    constexpr std::chrono::milliseconds calculate_interval(std::size_t subscribers, std::chrono::milliseconds max) {
        const auto sub = static_cast<double>(std::max(subscribers, static_cast<std::size_t>(1)) - 1);
        return std::min(
            max,
            std::max(MinimumInterval,
                     std::chrono::duration_cast<std::chrono::milliseconds>(MinimumInterval * std::sqrt(sub) * LoadFactor)));
    }

} // namespace constellation::protocol::CHP
