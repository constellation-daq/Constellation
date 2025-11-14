/**
 * @file
 * @brief Implementation of configuration YAML helpers
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "yaml_helpers.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "constellation/controller/exceptions.hpp"
#include "constellation/core/config/value_types.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::utils;

std::string constellation::controller::parse_yaml_key(const YAML::detail::iterator_value& iterator) {
    try {
        return transform(iterator.first.as<std::string>(), ::tolower);
    } catch(const YAML::Exception&) {
        throw ConfigParseError("keys need to be strings");
    }
}

// NOLINTBEGIN(misc-no-recursion)
Dictionary constellation::controller::parse_yaml_map(const std::string& key, const YAML::Node& node) {
    Dictionary dictionary {};
    const auto key_prefix = key + '.';
    for(const auto& sub_node : node) {
        const auto yaml_key_lc = transform(parse_yaml_key(sub_node), ::tolower);
        const auto [it, inserted] =
            dictionary.try_emplace(yaml_key_lc, parse_yaml_value(key_prefix + yaml_key_lc, sub_node.second));
        if(!inserted) {
            throw ConfigKeyError(key_prefix + yaml_key_lc, "key defined twice");
        }
    }
    return dictionary;
}
// NOLINTEND(misc-no-recursion)

namespace {
    template <typename T> bool decode_yaml_array(std::string_view key, const YAML::Node& node, std::vector<T>& rhs) {
        rhs.clear();
        bool first_element = true;
        T rv;
        for(const auto& element : node) {
            const auto success = YAML::convert<T>::decode(element, rv);
            if(first_element) {
                // Return early if decoding failed on first element
                if(!success) {
                    return false;
                }
                first_element = false;
            } else {
                // Inhomogeneous array if decoding failed on subsequent arrays
                if(!success) {
                    throw ConfigValueError(key, "array is not homogeneous");
                }
            }
            rhs.emplace_back(std::move(rv));
        }
        return true;
    }
} // namespace

// NOLINTBEGIN(misc-no-recursion)
Composite constellation::controller::parse_yaml_value(const std::string& key, const YAML::Node& node) {
    if(node.IsScalar()) {
        bool rv_bool {};
        if(YAML::convert<bool>::decode(node, rv_bool)) {
            return {rv_bool};
        }
        std::int64_t rv_int {};
        if(YAML::convert<std::int64_t>::decode(node, rv_int)) {
            return {rv_int};
        }
        double rv_float {};
        if(YAML::convert<double>::decode(node, rv_float)) {
            return {rv_float};
        }

        // TODO(stephan.lachnit): add chrono parsing

        // Otherwise return as string
        return node.as<std::string>();
    }

    if(node.IsSequence()) {
        std::vector<bool> rv_bool;
        if(decode_yaml_array(key, node, rv_bool)) {
            return {rv_bool};
        }
        std::vector<std::int64_t> rv_int;
        if(decode_yaml_array(key, node, rv_int)) {
            return {rv_int};
        }
        std::vector<double> rv_float {};
        if(decode_yaml_array(key, node, rv_float)) {
            return {rv_float};
        }

        // TODO(stephan.lachnit): add chrono parsing

        // Otherwise return as string
        return node.as<std::vector<std::string>>();
    }

    if(node.IsMap()) {
        return {parse_yaml_map(key, node)};
    }

    // If not returned yet then unknown type
    throw ConfigValueError(key, "unknown type");
}
// NOLINTEND(misc-no-recursion)

namespace {
    auto convert_for_yaml(auto&& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr(std::same_as<T, std::chrono::system_clock::time_point>) {
            return to_string(value);
        } else if constexpr(std::same_as<T, std::vector<bool>::const_reference>) {
            const bool rv = value;
            return rv;
        } else {
            return value;
        }
    }
} // namespace

// NOLINTBEGIN(misc-no-recursion)
YAML::Node constellation::controller::get_as_yaml_node(const Composite& value) {
    YAML::Node node {};
    std::visit(
        [&node](const auto& value_c) {
            using T = std::decay_t<decltype(value_c)>;
            if constexpr(std::same_as<T, Scalar>) {
                std::visit(
                    [&node](const auto& value_s) {
                        using U = std::decay_t<decltype(value_s)>;
                        if constexpr(!std::same_as<U, std::monostate>) {
                            node = convert_for_yaml(value_s);
                        }
                    },
                    value_c);
            } else if constexpr(std::same_as<T, Array>) {
                std::visit(
                    [&node](const auto& value_a) {
                        using U = std::decay_t<decltype(value_a)>;
                        if constexpr(!std::same_as<U, std::monostate>) {
                            std::ranges::for_each(value_a, [&node](const auto& v) { node.push_back(convert_for_yaml(v)); });
                        } else {
                            // If monostate, set to empty vector
                            node = std::vector<int>();
                        }
                    },
                    value_c);
            } else if constexpr(std::same_as<T, Dictionary>) {
                // Set key-value pairs recursively
                for(const auto& [key, value] : value_c) {
                    node[key] = get_as_yaml_node(value);
                }
            } else {
                std::unreachable();
            }
        },
        value);
    return node;
}
// NOLINTEND(misc-no-recursion)
