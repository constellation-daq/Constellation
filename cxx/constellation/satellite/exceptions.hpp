/**
 * @file
 * @brief Satellite exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <exception>
#include <string>

namespace constellation::satellite {

    /** Error thrown when a FSM transition is not allowed */
    class FSMError : public std::exception {
    public:
        /**
         * @param error_message Error message
         */
        FSMError(std::string error_message) : error_message_(std::move(error_message)) {}

        /**
         * @return Error message
         */
        const char* what() const noexcept final { return error_message_.c_str(); }

    private:
        std::string error_message_;
    };

} // namespace constellation::satellite
