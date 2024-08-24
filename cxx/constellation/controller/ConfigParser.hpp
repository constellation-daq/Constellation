/**
 * @file
 * @brief Configuration parser class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>

#include "constellation/build.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::controller {

    /**
     * @brief Configuration parser to read TOML files and emit dictionaries for individual satellites
     *
     * The configuration file holds a hierarchy of tables which contain the configuration keys for all satellites of the
     * Constellation. The dictionaries for the individual satellites need to be assembled from keys specific to the
     * respective satellite, keys valid for the relevant satellite type and keys intended for all satellites.
     */
    class CNSTLN_API ConfigParser {
    public:
        /// @cond doxygen_suppress
        ConfigParser() = default;
        ~ConfigParser() = default;
        ConfigParser(const ConfigParser& other) = delete;
        ConfigParser& operator=(const ConfigParser& other) = delete;
        ConfigParser(ConfigParser&& other) noexcept = delete;
        ConfigParser& operator=(ConfigParser&& other) = delete;
        /// @endcond

        /**
         * @brief Parse configuration and prepare configuration dictionary for a given satellite
         *
         * The TOML parse tree is specifically searched for the given satellite and its type because the TOML format is
         * case-sensitive and we need insensitive matches.
         *
         * @param satellite Satellite to obtain configuration dictionary for
         * @param toml String view of the TOML configuration file contents
         * @return Optional with configuration dictionary if satellite was found in configuration
         * @throws std::invalid_argument if no dictionary is available for this satellite or config parsing issues
         */
        static std::optional<config::Dictionary> getDictionary(const std::string& satellite, std::string_view toml);

        /**
         * @brief Parse configuration and prepare configuration dictionary for a given satellite
         *
         * The TOML parse tree is specifically searched for the given satellite and its type because the TOML format is
         * case-sensitive and we need insensitive matches.
         *
         * @param satellite Satellite to obtain configuration dictionary for
         * @param file Input file stream of the TOML configuration file
         * @return Optional with configuration dictionary if satellite was found in configuration
         * @throws std::invalid_argument if no dictionary is available for this satellite or config parsing issues
         */
        static std::optional<config::Dictionary> getDictionary(const std::string& satellite, std::ifstream file);

        /**
         * @brief Parse configuration and prepare configuration dictionary for a set of satellites
         *
         * It is necessary to also provide the set of satellites to parse this configuration for, since the TOML parse tree
         * is specifically searched for those satellites and types because the TOML format is case-sensitive and we need
         * insensitive matches.
         *
         * @param satellites Satellite to obtain configuration dictionary for
         * @param toml String view of the TOML configuration file contents
         * @return Map of all requested satellites that were found in the configuration
         * @throws std::invalid_argument if no dictionary is available for this satellite or config parsing issues
         */
        static std::map<std::string, config::Dictionary> getDictionaries(std::set<std::string> satellites,
                                                                         std::string_view toml);

        /**
         * @brief Parse configuration and prepare configuration dictionary for a set of satellites
         *
         * It is necessary to also provide the set of satellites to parse this configuration for, since the TOML parse tree
         * is specifically searched for those satellites and types because the TOML format is case-sensitive and we need
         * insensitive matches.
         *
         * @param satellites Satellite to obtain configuration dictionary for
         * @param file Input file stream of the TOML configuration file
         * @return Map of all requested satellites that were found in the configuration
         * @throws std::invalid_argument if no dictionary is available for this satellite or config parsing issues
         */
        static std::map<std::string, config::Dictionary> getDictionaries(std::set<std::string> satellites,
                                                                         std::ifstream file);

    private:
        static std::map<std::string, config::Dictionary> parse_config(std::set<std::string> satellites,
                                                                      std::string_view toml);

        static std::string read_file(std::ifstream file);

        /* Logger */
        static log::Logger config_parser_logger_;
    };

} // namespace constellation::controller
