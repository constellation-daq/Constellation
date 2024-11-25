/**
 * @file
 * @brief Network communication exceptions used in the framework
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <exception>
#include <string>
#include <utility>

#include "constellation/core/utils/exceptions.hpp"

namespace constellation::networking {

    /**
     * @ingroup Exceptions
     * @brief Errors related to network communication
     *
     * Problems that could never have been detected at compile time
     */
    class CNSTLN_API NetworkError : public utils::RuntimeError {
    public:
        /**
         * @brief Creates exception with the given network problem
         * @param what_arg Text describing the problem
         */
        explicit NetworkError(std::string what_arg) : RuntimeError(std::move(what_arg)) {}

    protected:
        /**
         * @brief Internal constructor for exceptions setting the error message indirectly
         */
        NetworkError() = default;
    };

} // namespace constellation::networking
