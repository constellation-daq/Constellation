/**
 * @file
 * @brief Satellite exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/exceptions.hpp"

namespace constellation::satellite {

    /**
     * @ingroup Exceptions
     * @brief Finite State Machine Error
     *
     * An error occurred in a request to the finite state machine
     */
    class FSMError : public utils::RuntimeError {
        explicit FSMError(const std::string& reason) { error_message_ = std::move(reason); }

    protected:
        FSMError() = default;
    };

    /**
     * @ingroup Exceptions
     * @brief Invalid transition requested
     *
     * A transition of the finite state machine was requested which is not allowed from the current state
     */
    class InvalidFSMTransition : public FSMError {
    public:
        explicit InvalidFSMTransition(const message::Transition transition, const message::State state) {
            error_message_ = "Transition ";
            error_message_ += utils::to_string(transition);
            error_message_ += " not allowed from state ";
            error_message_ += utils::to_string(state);
        }
    };
} // namespace constellation::satellite
