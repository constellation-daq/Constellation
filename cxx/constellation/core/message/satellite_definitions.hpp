/**
 * @file
 * @brief Protocol definitions for the satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

#include <magic_enum.hpp>

#include "constellation/core/utils/std23.hpp"

namespace constellation::message {

    /** Possible Satellite FSM states */
    enum class State : std::uint8_t {
        NEW = 0x10,
        initializing = 0x12,
        INIT = 0x20,
        launching = 0x23,
        ORBIT = 0x30,
        landing = 0x32,
        reconfiguring = 0x33,
        starting = 0x34,
        RUN = 0x40,
        stopping = 0x43,
        interrupting = 0x0E,
        SAFE = 0xE0,
        ERROR = 0xF0,
    };

    /** Possible FSM transitions */
    enum class Transition : std::uint8_t {
        initialize,
        initialized,
        launch,
        launched,
        land,
        landed,
        reconfigure,
        reconfigured,
        start,
        started,
        stop,
        stopped,
        interrupt,
        interrupted,
        failure,
    };

    /** Possible transition commands via CSCP */
    enum class TransitionCommand : std::underlying_type_t<Transition> {
        initialize = std::to_underlying(Transition::initialize),
        launch = std::to_underlying(Transition::launch),
        land = std::to_underlying(Transition::land),
        reconfigure = std::to_underlying(Transition::reconfigure),
        start = std::to_underlying(Transition::start),
        stop = std::to_underlying(Transition::stop),
    };

    /** Possible get_* commands via CSCP */
    enum class GetCommand : std::underlying_type_t<TransitionCommand> {
        get_name,
        get_commands,
        get_state,
        get_status,
        get_config,
    };

} // namespace constellation::message

// State enum exceeds default enum value limits of magic_enum (-128, 127)
namespace magic_enum::customize {
    template <> struct enum_range<constellation::message::State> {
        static constexpr int min = 0;
        static constexpr int max = 255;
    };
} // namespace magic_enum::customize
