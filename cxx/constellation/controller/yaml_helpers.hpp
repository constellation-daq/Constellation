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

    std::string parse_yaml_key(const YAML::detail::iterator_value& iterator);

    config::Dictionary parse_yaml_map(const std::string& key, const YAML::Node& node);

    config::Composite parse_yaml_value(const std::string& key, const YAML::Node& node);

    YAML::Node get_as_yaml_node(const config::Composite& value);

} // namespace constellation::controller
