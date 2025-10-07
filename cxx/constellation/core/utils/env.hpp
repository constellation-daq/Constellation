/**
 * @file
 * @brief Environment variable wrappers
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdlib>
#include <optional>
#include <string>

namespace constellation::utils {

    /**
     * @brief Wrapper for std::getenv to read environment variables
     * @details This helper reads environment variables. If a default is provided and the variable could not be found, the
     *          default is returned
     *
     * @param name Name of the environment variable
     * @param default_val Fallback value in case the environment variable is not found
     * @return Optional with the valure read from the environment variable
     */
    inline std::optional<std::string> getenv(const std::string& name, const std::string default_val = {}) {
        const auto* val = std::getenv(name.c_str());
        if(val == nullptr) {
            if(!default_val.empty()) {
                return default_val;
            }
            return std::nullopt;
        }
        return std::string(val);
    }

} // namespace constellation::utils
