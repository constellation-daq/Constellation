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
#include <ranges>
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
        const std::scoped_lock<std::mutex> lock(getenv_mutex);

        const auto* val = std::getenv(name.c_str()); // NOLINT(concurrency-mt-unsafe)
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
     * @param pattern Input regular expression. The first match group represents the variable name, the second an optional
     * default
     * @param input Input string to resolve matches for
     *
     * @return String with all regular expression matches replaced
     * @throws RuntimeError if an environment variable could not be found
     */
    inline std::string resolve_env(const std::regex& pattern, const std::string& input) {

        const std::sregex_iterator begin(input.begin(), input.end(), pattern);
        const std::sregex_iterator end {};
        std::size_t last_pos = 0;

        std::string result;
        for(const auto& match : std::ranges::subrange(begin, end)) {
            result += input.substr(last_pos, match.position() - last_pos);
            // Reinsert matched prefix character
            if(match[1].matched) {
                result += match[1].str();
            }
            auto env_val = getenv(match[2]);
            if(env_val.has_value()) {
                result += env_val.value();
            } else if(match[3].matched) {
                result += match[3].str();
            } else {
                throw RuntimeError("Environment variable " + quote(match[2].str()) + " not defined");
            }
            last_pos = match.position() + match.length();
        }

        result += input.substr(last_pos);
        return result;
    }

    /**
     * @brief Helper to resolve all controller environment variables matching _${VAR}.
     * @details This method matches strings following the pattern `_${}` and attempts to replace them with an environment
     *          variable of the same name. It respects escaping of the pattern via `\_` and will replace this escape sequence
     *          with `_` after resolution of the environment variables.
     *
     * @param config_value Input string to resolve environment variables in
     * @return String with all environment variables replaced with their values
     * @throws RuntimeError if an environment variable could not be found
     */
    inline std::string resolve_controller_env(const std::string& config_value) {
        const std::regex ctrl_pattern(R"((^|[^\\])_\$\{(\w+)(?::-([^}]*))?\})");

        // Resolve environment variables
        const auto resolved = resolve_env(ctrl_pattern, config_value);
        // Resolve escape sequence
        return std::regex_replace(resolved, std::regex(R"(\\_)"), "_");
    }

    /**
     * @brief Helper to resolve all satellite environment variables matching ${VAR}
     * @details This method matches strings following the pattern `${}` and attempts to replace them with an environment
     *          variable of the same name. It respects escaping of the pattern via `\$` and will replace this escape sequence
     *          with `$` after resolution of the environment variables.
     *
     * @param config_value Input string to resolve environment variables in
     * @return String with all environment variables replaced with their values
     * @throws RuntimeError if an environment variable could not be found
     */
    inline std::string resolve_satellite_env(const std::string& config_value) {
        const std::regex sat_pattern(R"((^|[^\\])\$\{(\w+)(?::-([^}]*))?\})");

        // Resolve environment variables
        const auto resolved = resolve_env(sat_pattern, config_value);
        // Resolve escape sequence
        return std::regex_replace(resolved, std::regex(R"(\\\$)"), "$");
    }

} // namespace constellation::utils
