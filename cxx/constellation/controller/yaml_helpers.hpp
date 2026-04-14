/**
 * @file
 * @brief Configuration YAML helpers
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>

#include <yaml-cpp/yaml.h>

#include "constellation/core/config/value_types.hpp"

namespace constellation::controller {

    /**
     * @brief Parse key as string and transform to lower-case
     *
     * @param iterator YAML node iterator
     * @return Lower-case string of the key
     */
    std::string parse_yaml_key(const YAML::detail::iterator_value& iterator);

    /**
     * @brief Parse YAML node as map
     *
     * @param key Map key used as prefix
     * @param node YAML node to be parsed
     *
     * @return Dictionary
     */
    config::Dictionary parse_yaml_map(const std::string& key, const YAML::Node& node);

    /**
     * @brief Parse YAML node into configuration value
     *
     * @param key Key of the parameter
     * @param node YAML node to be parsed
     *
     * @return Configuration value composite
     */
    config::Composite parse_yaml_value(const std::string& key, const YAML::Node& node);

    /**
     * @brief Convert configuration composite value into YAML node
     *
     * @param value Value to be converted
     * @return YAML node holding the input value
     */
    YAML::Node get_as_yaml_node(const config::Composite& value);

    /**
     * \brief Helper to parse integers with binary (0b) or octal (0o) prefixes
     *
     * @param node YAML node to be parsed
     * @param value Integer value
     *
     * @return True if conversion succeeded, false otherwise
     */
    bool parse_yaml_int_with_prefix(const YAML::Node& node, std::int64_t& value);

} // namespace constellation::controller
