/**
 * @file
 * @brief FSM enums
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "constellation/core/message/satellite_definitions.hpp"

namespace constellation::satellite {

    // Forward possible FSM states
    using message::State;

    // Forward possible FSM transitions
    using message::Transition;

    // Forward possible FSM transition commands
    using message::TransitionCommand;

} // namespace constellation::satellite
