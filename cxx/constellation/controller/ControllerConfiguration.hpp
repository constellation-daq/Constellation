/**
 * @file
 * @brief Configuration parser class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "constellation/build.hpp"
#include "constellation/core/config/value_types.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string_hash_map.hpp"

namespace constellation::controller {

    /**
     * @brief Configuration parser to read configuration files and emit dictionaries for individual satellites
     *
     * The configuration file holds a hierarchy of tables which contain the configuration keys for all satellites of the
     * Constellation. The dictionaries for the individual satellites need to be assembled from keys specific to the
     * respective satellite, keys valid for the relevant satellite type and keys intended for all satellites.
     */
    class ControllerConfiguration {
    public:
        /** File type of the configuration */
        enum class FileType : std::uint8_t {
            /** Unknown configuration file type */
            UNKNOWN,
            /** TOML configuration file */
            TOML,
            /** YAML configuration file */
            YAML,
        };

        /**
         * @brief Default constructor with empty configuration
         */
        ControllerConfiguration() = default;

        /**
         * @brief Construct a controller configuration and parse dictionaries from a string
         *
         * @param config Configuration data as string
         * @param type Type of configuration to be parsed
         *
         * @throws ConfigFileNotFoundError if the configuration file could not be found or opened
         * @throws ConfigFileParseError if the configuration file could not be parsed as valid file format
         * @throws ConfigFileTypeError if the configuration file contained invalid value types
         */
        CNSTLN_API ControllerConfiguration(std::string_view config, FileType type);

        /**
         * @brief Construct a controller configuration and parse dictionaries from a configuration file
         *
         * @param path File path to the configuration file
         *
         * @throws ConfigFileNotFoundError if the configuration file could not be found or opened
         * @throws ConfigFileParseError if the configuration file could not be parsed as valid file format
         * @throws ConfigFileTypeError if the configuration file contained invalid value types
         */
        CNSTLN_API explicit ControllerConfiguration(const std::filesystem::path& path);

        ~ControllerConfiguration() = default;

        /// @cond doxygen_suppress
        ControllerConfiguration(const ControllerConfiguration& other) = delete;
        ControllerConfiguration& operator=(const ControllerConfiguration& other) = delete;
        ControllerConfiguration(ControllerConfiguration&& other) noexcept = delete;
        ControllerConfiguration& operator=(ControllerConfiguration&& other) = delete;
        /// @endcond

        /**
         * @brief Set the global configuration
         *
         * @note This method always overwrites the current global configuration without warning
         *
         * @param config Dictionary with the global configuration
         */
        void setGlobalConfiguration(config::Dictionary config) { global_config_ = std::move(config); }

        /**
         * @brief Get the configuration at the global level
         *
         * @returns Dictionary with the global configuration
         */
        const config::Dictionary& getGlobalConfiguration() const { return global_config_; };

        /**
         * @brief Check if an explicit configuration exists for a given satellite type
         *
         * @returns True if a configuration is available for the given satellite type, false otherwise
         */
        CNSTLN_API bool hasTypeConfiguration(std::string_view type) const;

        /**
         * @brief Add an explicit configuration for a satellite type
         *
         * @param type Satellite type to add the configuration for
         * @param config Dictionary with the satellite type configuration
         */
        CNSTLN_API void addTypeConfiguration(std::string_view type, config::Dictionary config);

        /**
         * @brief Get configuration for a given satellite type
         *
         * @param type Satellite type to get the configuration for
         * @returns Dictionary with the combined configuration for the given satellite type
         */
        CNSTLN_API config::Dictionary getTypeConfiguration(std::string_view type) const;

        /**
         * @brief Check if an explicit configuration exists for a given satellite
         *
         * The cached dictionaries from the input configuration are searched for the given satellite type, and keys from the
         * the type level as global keys to all satellites are added.
         *
         * @param canonical_name Canonical name of the satellite
         * @return True if a configuration is available for the satellite with the given canonical name, false otherwise
         */
        CNSTLN_API bool hasSatelliteConfiguration(std::string_view canonical_name) const;

        /**
         * @brief Add an explicit configuration for a satellite
         *
         * @param canonical_name Canonical name of the satellite to add the configuration for
         * @param config Dictionary with the satellite configuration
         */
        CNSTLN_API void addSatelliteConfiguration(std::string_view canonical_name, config::Dictionary config);

        /**
         * @brief Get configuration for a given satellite
         *
         * The cached dictionaries from the input configuration are searched for the given satellite, and keys from the
         * type level matching this satellite's type as well as global keys to all satellites are added.
         *
         * @param canonical_name Canonical name of the satellite
         * @return Dictionary with the combined configuration for the given satellite
         */
        CNSTLN_API config::Dictionary getSatelliteConfiguration(std::string_view canonical_name) const;

        /**
         * @brief Get configuration as TOML
         *
         * @return String containing the configuration as TOML
         */
        CNSTLN_API std::string getAsTOML() const;

        /**
         * @brief Get configuration as YAML
         *
         * @return String containing the configuration as YAML
         */
        CNSTLN_API std::string getAsYAML() const;

        /**
         * @brief Validate the configuration
         * @details Runs checks such as dependency graph validation on the set of satellite configurations
         *
         * @throws ConfigFileValidationError if a validation error is encountered
         */
        CNSTLN_API void validate();

    private:
        static FileType detect_config_type(const std::filesystem::path& file);

        /**
         * @brief Parse a string view with TOML data into dictionaries
         *
         * @param toml TOML data as string
         *
         * @throws ConfigFileNotFoundError if the configuration file could not be found or opened
         * @throws ConfigFileParseError if the configuration file could not be parsed into valid TOML
         * @throws ConfigFileTypeError if the configuration file contained invalid value types
         */
        void parse_toml(std::string_view toml);

        /**
         * @brief Parse a string view with YAML data into dictionaries
         *
         * @param yaml YAML data as string
         *
         * @throws ConfigFileNotFoundError if the configuration file could not be found or opened
         * @throws ConfigFileParseError if the configuration file could not be parsed into valid YAML
         * @throws ConfigFileTypeError if the configuration file contained invalid value types
         */
        void parse_yaml(std::string_view yaml);

        /**
         * @brief Add satellite dependencies to the transition dependency graph
         * @details This method looks for keys of the autonomous transition orchestration on the final assembled satellite
         *          configuration and adds the respective dependencies to the graph for validation. All satellite names are
         *          canonical names in lower case.
         *
         * @param canonical_name Canonical name of the satellite to be added to the dependency graph
         */
        void fill_dependency_graph(std::string_view canonical_name);

        /**
         * @brief Helper function to check for deadlock in a specific transition
         * @details This method traverses the dependency graph for the given transition and checks for cycles
         *
         * @param transition Transition to be checked
         * @return Boolean indicating whether a cycle has been found
         */
        bool check_transition_deadlock(protocol::CSCP::State transition) const;

        void overwrite_config(const std::string& key_prefix,
                              config::Dictionary& base_config,
                              const config::Dictionary& config) const;

    private:
        /* Key-value pairs of the global level */
        config::Dictionary global_config_;

        /* Dictionaries of satellite type level */
        utils::string_hash_map<config::Dictionary> type_configs_;

        /**
         * Dictionaries for individual satellites
         *
         * @note The keys here are the full canonical names of the satellites since the same name for different-type
         *       satellites are allowed
         */
        utils::string_hash_map<config::Dictionary> satellite_configs_;

        /* Satellite dependency graph for each transition type */
        std::unordered_map<protocol::CSCP::State, utils::string_hash_map<std::set<std::string>>> transition_graph_;

        /* Logger */
        log::Logger config_parser_logger_ {"CTRL"};
    };

} // namespace constellation::controller
