/**
 * @file
 * @brief Inline implementation of configuration TOML helpers
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "toml_helpers.hpp" // NOLINT(misc-header-include-cycle)

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

#include "constellation/controller/exceptions.hpp"
#include "constellation/core/config/value_types.hpp"

namespace constellation::controller {

    template <typename T, typename F> config::Array convert_toml_array(const toml::array& array, F op) {
        std::vector<T> out {};
        out.reserve(array.size());
        for(const auto& element : array) {
            out.push_back(op(element));
        }
        return {out};
    }

    // NOLINTBEGIN(misc-no-recursion)
    config::Composite parse_toml_value(const std::string& key, auto&& value) {
        using T = decltype(value);

        if constexpr(toml::is_boolean<T> || toml::is_integer<T> || toml::is_floating_point<T> || toml::is_string<T>) {
            return {value.get()};
        }
        if constexpr(toml::is_time<T>) {
            return {from_toml_time(value.get())};
        }

        if constexpr(toml::is_array<T>) {
            // Check for empty array first
            if(value.empty()) {
                return {config::Array()};
            }
            // Throw if inhomogeneous array
            if(!value.is_homogeneous()) {
                throw ConfigValueError(key, "array is not homogeneous");
            }
            // Check first entry
            if(value.front().is_boolean()) {
                return {convert_toml_array<bool>(value, [](const auto& element) { return element.as_boolean()->get(); })};
            }
            if(value.front().is_integer()) {
                return {convert_toml_array<std::int64_t>(value,
                                                         [](const auto& element) { return element.as_integer()->get(); })};
            }
            if(value.front().is_floating_point()) {
                return {convert_toml_array<double>(value,
                                                   [](const auto& element) { return element.as_floating_point()->get(); })};
            }
            if(value.front().is_string()) {
                return {
                    convert_toml_array<std::string>(value, [](const auto& element) { return element.as_string()->get(); })};
            }
            if(value.front().is_time()) {
                return {convert_toml_array<std::chrono::system_clock::time_point>(
                    value, [](const auto& element) { return from_toml_time(element.as_time()->get()); })};
            }
        }

        if constexpr(toml::is_table<T>) {
            return {parse_toml_table(key, value)};
        }

        // If not returned yet then unknown type
        throw ConfigValueError(key, "unknown type");
    }
    // NOLINTEND(misc-no-recursion)

} // namespace constellation::controller
