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
        /**
         * @brief Constructor which reads and parses a TOML configuration
         *
         * It is necessary to also provide the set of satellites to parse this configuration for, since the TOML parse tree
         * is specifically searched for those satellites and types because the TOML format is case-sensitive and we need
         * insensitive matches.
         *
         * @param toml String view of the TOML configuration file contents
         * @param satellites Set of canonical names of the satellites
         * @throws invalid_arguments in case of parsing issues.
         */
        ConfigParser(std::string_view toml, std::set<std::string> satellites);

        /**
         * @brief Constructor which reads and parses a TOML configuration file
         *
         * It is necessary to also provide the set of satellites to parse this configuration for, since the TOML parse tree
         * is specifically searched for those satellites and types because the TOML format is case-sensitive and we need
         * insensitive matches.
         *
         * @param file Input file stream of the TOML configuration file
         * @param satellites Set of canonical names of the satellites
         * @throws invalid_arguments in case of parsing issues.
         */
        ConfigParser(std::ifstream file, std::set<std::string> satellites);

        /// @cond doxygen_suppress
        ConfigParser() = delete;
        ~ConfigParser() = default;
        ConfigParser(const ConfigParser& other) = delete;
        ConfigParser& operator=(const ConfigParser& other) = delete;
        ConfigParser(ConfigParser&& other) noexcept = delete;
        ConfigParser& operator=(ConfigParser&& other) = delete;
        /// @endcond

        /**
         * @brief Check if a config for a specific satellite has been parsed and is available
         *
         * @param satellite Satellite to look for
         * @return True if a configuration dictionary is found, false otherwise
         */
        bool hasConfig(const std::string& satellite) const {
            return configs_.contains(utils::transform(satellite, ::tolower));
        }

        /**
         * @brief Get configuration dictionary for a given satellite
         *
         * @param satellite Satellite to obtain configuration dictionary for
         * @return configuration dictionary
         * @throws std::invalid_argument if no dictionary is available for this satellite.
         */
        config::Dictionary getConfig(const std::string& satellite) const;

        /**
         * @brief Return all parsed dictionaries as a map
         * @return Map of all satellites and their configuration dictionaries
         */
        std::map<std::string, config::Dictionary> getAll() { return configs_; }

    private:
        /* Logger */
        log::Logger logger_;

        /* Map of parsed configuration dictionaries */
        std::map<std::string, config::Dictionary> configs_;
    };

} // namespace constellation::controller
