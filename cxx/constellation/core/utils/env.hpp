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

    /**
     * @brief Helper to resolve all environment variables in a string
     * @details This method takes a regex pattern and an input string. For each match, the first match group is taken and
     *          looked up as environment variable. A runtime exception is thrown if not found, the match string replaced with
     *          the value otherwise.
     *
     * @param pattern Input regular expression. Only the first match group is used.
     * @param input Input string to resolve matches for
     *
     * @return String with all regular expression matches replaced
     * @throws RuntimeError if an environment variable could not be found
     */
    inline std::string resolve_env(const std::regex& pattern, const std::string& input) {

        std::sregex_iterator begin(input.begin(), input.end(), pattern);
        std::sregex_iterator end {};
        std::size_t last_pos = 0;

        std::string result;
        for(const auto& match : std::ranges::subrange(begin, end)) {
            result += input.substr(last_pos, match.position() - last_pos);
            auto env_val = getenv(match[1]);
            if(!env_val.has_value()) {
                throw RuntimeError("Environment variable " + quote(match[1].str()) + " not defined");
            }
            result += env_val.value();
            last_pos = match.position() + match.length();
        }

        result += input.substr(last_pos);
        return result;
    }

    /**
     * @brief Helper to resolve all controller environment variables matching _${VAR} or _$VAR
     *
     * @param config_value Input string to resolve environment variables in
     * @return String with all environment variables replaced with their values
     * @throws RuntimeError if an environment variable could not be found
     */
    inline std::string resolve_controller_env(const std::string& config_value) {
        std::regex ctrl_pattern(R"(_\$(?:\{|\b)(\w+)(?:\}|\b))");
        return resolve_env(ctrl_pattern, config_value);
    }

    /**
     * @brief Helper to resolve all satellite environment variables matching ${VAR} or $VAR
     *
     * @param config_value Input string to resolve environment variables in
     * @return String with all environment variables replaced with their values
     * @throws RuntimeError if an environment variable could not be found
     */
    inline std::string resolve_satellite_env(const std::string& config_value) {
        std::regex sat_pattern(R"(\$(?:\{|\b)(\w+)(?:\}|\b))");
        return resolve_env(sat_pattern, config_value);
    }

} // namespace constellation::utils
