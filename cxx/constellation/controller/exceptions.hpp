/**
 * @file
 * @brief Controller & ConfigParser exceptions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>
#include <string_view>

#include "constellation/build.hpp"
#include "constellation/core/utils/exceptions.hpp"

namespace constellation::controller {
    /**
     * @ingroup Exceptions
     * @brief Base class for all controller exceptions
     */
    class CNSTLN_API ControllerError : public utils::RuntimeError {};

    /**
     * @ingroup Exceptions
     * @brief Notifies of a missing configuration file
     */
    class CNSTLN_API ConfigFileNotFoundError : public ControllerError {
    public:
        /**
         * @brief Construct an error for a configuration that is not found
         * @param file_name Name of the configuration file
         */
        explicit ConfigFileNotFoundError(const std::filesystem::path& file_name) {
            error_message_ = "Could not read configuration file ";
            error_message_ += file_name;
        }
    };

} // namespace constellation::controller
