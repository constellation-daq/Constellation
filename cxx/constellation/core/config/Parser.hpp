/**
 * @file
 * @brief Configuration parser class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <filesystem>
#include <istream>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Value.hpp"

namespace constellation::config {

    /**
     * @brief Reader of configuration files
     *
     * Read the internal configuration file format used in the framework. The format contains
     * - A set of section header between [ and ] brackets
     * - Key/value pairs linked to the last defined section (or the empty section if none has been defined yet)
     */
    class CNSTLN_API ConfigParser {
    public:
        /**
         * @brief Constructs a config reader without any attached streams
         */
        ConfigParser();

        /**
         * @brief Constructs a config reader with a single attached stream
         * @param stream Stream to read configuration from
         * @param file_name Name of the file related to the stream or empty if not linked to a file
         */
        explicit ConfigParser(std::istream& stream, std::filesystem::path file_name = "");

        /**
         * @brief Parse a line as key-value pair
         * @param line Line to interpret
         * @return Pair of the key and the value
         */
        static std::pair<std::string, Value> parseKeyValue(std::string line);

        /**
         * @brief Adds a configuration stream to read
         * @param stream Stream to read configuration from
         * @param file_name Name of the file related to the stream or empty if not linked to a file
         */
        void add(std::istream&, std::filesystem::path file_name = "");

        /**
         * @brief Directly add a configuration object to the reader
         * @param config Configuration object to add
         */
        void addConfiguration(const std::string& name, Configuration config);

        /// @{
        /**
         * @brief Implement correct copy behaviour
         */
        ConfigParser(const ConfigParser&) = delete;
        ConfigParser& operator=(const ConfigParser&) = delete;
        /// @}

        /// @{
        /**
         * @brief Use default move behaviour
         */
        ConfigParser(ConfigParser&&) noexcept = default;
        ConfigParser& operator=(ConfigParser&&) noexcept = default;
        /// @}

        /**
         * @brief Removes all streams and all configurations
         */
        void clear();

        /**
         * @brief Check if a configuration exists
         * @param name Name of a configuration header to search for
         * @return True if at least a single configuration with this name exists, false otherwise
         */
        bool hasConfiguration(const std::string& name) const;
        /**
         * @brief Count the number of configurations with a particular name
         * @param name Name of a configuration header
         * @return The number of configurations with the given name
         */
        unsigned int countConfigurations(const std::string& name) const;

        /**
         * @brief Get all configurations with a particular header
         * @param name Header name of the configurations to return
         * @return List of configurations with the given name
         */
        std::vector<Configuration> getConfigurations(const std::string& name) const;

        /**
         * @brief Get all configurations
         * @return List of all configurations
         */
        std::vector<Configuration> getConfigurations() const;

    private:
        /**
         * @brief Node in a parse tree
         */
        struct parse_node {
            std::string value;
            std::vector<std::unique_ptr<parse_node>> children;
        };
        /**
         * @brief Generate parse tree from configuration string
         * @param str String to parse
         * @param depth Current depth of the parsing (starts at zero)
         * @return Root node of the parsed tree
         */
        static std::unique_ptr<parse_node> parse_value(std::string str, int depth = 0);

        std::map<std::string, std::vector<std::list<Configuration>::iterator>> conf_map_;
        std::list<Configuration> conf_array_;
    };
} // namespace constellation::config
