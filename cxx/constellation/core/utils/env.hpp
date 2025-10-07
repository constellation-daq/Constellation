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
#include <mutex>
#include <optional>
#include <regex>
#include <string>

#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::utils {

    /**
     * @brief Wrapper for std::getenv to read environment variables
     * @details This helper reads environment variables. If a default is provided and the variable could not be found, the
     *          default is returned
     *
     * @param name Name of the environment variable
     * @return Optional with the value read from the environment variable
     */
    inline std::optional<std::string> getenv(const std::string& name) {
        static std::mutex getenv_mutex;
        std::lock_guard<std::mutex> lock(getenv_mutex);

        const auto* val = std::getenv(name.c_str());
        if(val == nullptr) {
            return std::nullopt;
        }
        return std::string(val);
    }

} // namespace constellation::utils
