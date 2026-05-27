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
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <limits>
#include <regex>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "constellation/controller/exceptions.hpp"
#include "constellation/core/config/value_types.hpp"
#include "constellation/core/utils/env.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/time.hpp"

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

bool constellation::controller::parse_yaml_time(const YAML::Node& node, std::chrono::system_clock::time_point& rhs) {

    static const std::regex time_regex(R"((\d{2}):(\d{2}):(\d{2})(\.\d+)?)");
    static const std::regex date_regex(R"((\d{4})-(\d{2})-(\d{2}))");
    static const std::regex datetime_regex(
        R"((\d{4})-(\d{2})-(\d{2})[T ](\d{2}):(\d{2}):(\d{2})[\.]?(\d+)?(Z|[+-]\d{2}:\d{2})?)");

    // Chrono parsing:
    auto node_str = node.as<std::string>();
    std::smatch match;

    // Check if this is a daytime:
    if(std::regex_match(node_str, match, time_regex)) {
        // Convert to system time, ignore milliseconds
        rhs = localtime_to_system(std::stoi(match[1]), std::stoi(match[2]), std::stoi(match[3]));
        return true;
    }

    // Check for local date
    if(std::regex_match(node_str, match, date_regex)) {
        rhs = localdate_to_system(std::stoi(match[1]), std::stoi(match[2]), std::stoi(match[3]));
        return true;
    }

    // Check for datetime:
    if(std::regex_match(node_str, match, datetime_regex)) {
        rhs = datetime_to_system(std::stoi(match[1]),
                                 std::stoi(match[2]),
                                 std::stoi(match[3]),
                                 std::stoi(match[4]),
                                 std::stoi(match[5]),
                                 std::stoi(match[6]),
                                 match[8]);
        return true;
    }

    return false;
}

namespace {
    template <typename T> bool decode_yaml_array(std::string_view key, const YAML::Node& node, std::vector<T>& rhs) {
        rhs.clear();
        bool first_element = true;
        T rv;
        for(const auto& element : node) {
            bool success {false};
            if constexpr(std::is_same_v<T, std::chrono::system_clock::time_point>) {
                success = parse_yaml_time(element, rv);
            } else {
                success = YAML::convert<T>::decode(element, rv);
            }
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

bool constellation::controller::parse_yaml_binary_int(const YAML::Node& node, std::int64_t& out) {
    const std::string& input = node.Scalar();
    const auto* p = input.data();
    const auto* end = p + input.size();

    // Parse optional sign
    bool negative = false;
    if(p != end && (*p == '-' || *p == '+')) {
        negative = (*p++ == '-');
    }

    // Match "0b" / "0B" prefix
    if(end - p < 3 || *p != '0') {
        return false;
    }
    ++p;

    if(*p != 'b' && *p != 'B') {
        return false;
    }
    ++p;

    // Parse binary digits
    std::uint64_t value {};
    const auto [last, ec] = std::from_chars(p, end, value, 2);
    if(ec != std::errc {} || last != end) {
        return false;
    }

    // Assign to signed output, checking range
    if(negative) {
        const auto limit = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1;
        if(value > limit) {
            return false;
        }
        out = static_cast<std::int64_t>(~value + 1);
    } else {
        if(value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            return false;
        }
        out = static_cast<std::int64_t>(value);
    }
    return true;
}

// NOLINTBEGIN(misc-no-recursion)
Composite constellation::controller::parse_yaml_value(const std::string& key, const YAML::Node& node) {
    if(node.IsNull()) {
        // Interpret empty node as empty dictionary
        return Dictionary();
    }

    if(node.IsScalar()) {
        bool rv_bool {};
        if(YAML::convert<bool>::decode(node, rv_bool)) {
            return {rv_bool};
        }
        std::int64_t rv_int {};
        if(YAML::convert<std::int64_t>::decode(node, rv_int) || parse_yaml_binary_int(node, rv_int)) {
            return {rv_int};
        }
        double rv_float {};
        if(YAML::convert<double>::decode(node, rv_float)) {
            return {rv_float};
        }

        std::chrono::system_clock::time_point rv_time {};
        if(parse_yaml_time(node, rv_time)) {
            return {rv_time};
        }

        // Otherwise return as string after resolving environment variables
        try {
            return resolve_controller_env(node.as<std::string>());
        } catch(const RuntimeError& e) {
            throw ConfigValueError(key, e.what());
        }
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

        std::vector<std::chrono::system_clock::time_point> rv_time {};
        if(decode_yaml_array(key, node, rv_time)) {
            return {rv_time};
        }

        // Otherwise return as string vector after resolving environment variables
        auto retval = node.as<std::vector<std::string>>();
        try {
            std::ranges::transform(retval, retval.begin(), [](const auto& val) { return resolve_controller_env(val); });
        } catch(const RuntimeError& e) {
            throw ConfigValueError(key, e.what());
        }
        return retval;
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
            return to_rfc3339_string(value);
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
