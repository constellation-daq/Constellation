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

#include "constellation/core/utils/enum.hpp" // IWYU pragma: keep

namespace constellation::protocol::CHP {

    /** Default lives for a remote on detection/replenishment */
    static constexpr std::uint8_t Lives = 3;

    /** Possible CHP message flags */
    enum MessageFlags : std::uint8_t {
        NONE = 0x00,

        /** Indicating that the sender should not be allowed to depart, and an interrupt should be triggered */
        DENY_DEPARTURE = 0x01,

        /** Indicating that ERROR or SAFE states and missing heartbeats should trigger an interrupt */
        TRIGGER_INTERRUPT = 0x02,

        /** Indicating that the current run should me marked as degraded if this sender reports failure or disappears */
        MARK_DEGRADED = 0x04,

        /* Indicate a extrasystole message */
        IS_EXTRASYSTOLE = 0x80,
    };

    /** Satellite roles, representing a combination of message flags */
    enum class Role : std::uint8_t {
        NONE,      ///< No Flags
        TRANSIENT, ///< Flags MARK_DEGRADED
        DYNAMIC,   ///< Flags MARK_DEGRADED, TRIGGER_INTERRUPT
        ESSENTIAL  ///< Flags MARK_DEGRADED, TRIGGER_INTERRUPT, DENY_DEPARTURE
    };

    /**
     * @brief Get flags for a given role
     *
     * @param role Role
     */
    constexpr MessageFlags flags_from_role(Role role) {
        if(role == Role::TRANSIENT) {
            return MessageFlags::MARK_DEGRADED;
        }
        if(role == Role::DYNAMIC) {
            return MessageFlags::MARK_DEGRADED | MessageFlags::TRIGGER_INTERRUPT;
        }
        if(role == Role::ESSENTIAL) {
            return MessageFlags::MARK_DEGRADED | MessageFlags::TRIGGER_INTERRUPT | MessageFlags::DENY_DEPARTURE;
        }

        return MessageFlags::NONE;
    }

    /**
     * @brief Get role from given message flags
     *
     * @param flags Message flags
     */
    constexpr Role role_from_flags(MessageFlags flags) {
        if((flags & MessageFlags::MARK_DEGRADED) != 0U) {
            if((flags & MessageFlags::TRIGGER_INTERRUPT) != 0U) {
                if((flags & MessageFlags::DENY_DEPARTURE) != 0U) {
                    return Role::ESSENTIAL;
                }
                return Role::DYNAMIC;
            }
            return Role::TRANSIENT;
        }
        return Role::NONE;
    }

    /**
     * @brief Check if the given role requires the given message flag
     *
     * @param role Role
     * @param flags Message flags
     *
     * @return True if role mandates the flag, false otherwise
     */
    constexpr bool role_requires(Role role, MessageFlags flags) {
        return (flags_from_role(role) & flags) != 0U;
    }

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
