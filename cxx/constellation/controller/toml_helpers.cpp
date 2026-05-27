/**
 * @file
 * @brief Implementation of configuration TOML helpers
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "toml_helpers.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <concepts>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if __cpp_lib_chrono < 201907L
#include <ctime>
#endif

#include <toml++/toml.hpp>

#include "constellation/controller/exceptions.hpp"
#include "constellation/core/config/value_types.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::utils;

toml::date_time constellation::controller::to_toml_time(const std::chrono::system_clock::time_point& system_time) {
    const auto [date, time] = system_to_localdatetime(system_time);

    return toml::date_time(toml::date(static_cast<int>(date.year()),
                                      static_cast<unsigned int>(date.month()),
                                      static_cast<unsigned int>(date.day())),
                           toml::time(time.hours().count(), time.minutes().count(), time.seconds().count()));
}

// NOLINTBEGIN(misc-no-recursion)
Dictionary constellation::controller::parse_toml_table(const std::string& key, const toml::table& table) {
    Dictionary dictionary {};
    const auto key_prefix = key + '.';
    table.for_each([&dictionary, &key_prefix](const toml::key& toml_key, auto&& value) {
        const auto toml_key_lc = transform(toml_key.str(), ::tolower);
        const auto [it, inserted] = dictionary.try_emplace(
            toml_key_lc, parse_toml_value(key_prefix + toml_key_lc, std::forward<decltype(value)>(value)));
        if(!inserted) {
            throw ConfigKeyError(key_prefix + toml_key_lc, "key defined twice");
        }
    });
    return dictionary;
}
// NOLINTEND(misc-no-recursion)

namespace {
    auto convert_for_toml(auto&& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr(std::same_as<T, std::chrono::system_clock::time_point>) {
            return to_toml_time(value);
        } else if constexpr(std::same_as<T, std::vector<bool>::const_reference>) {
            const bool rv = value;
            return rv;
        } else {
            return value;
        }
    }
} // namespace

// NOLINTBEGIN(misc-no-recursion)
toml::table constellation::controller::get_as_toml_table(const Dictionary& dictionary) {
    toml::table table {};
    for(const auto& [key, value] : dictionary) {
        std::visit(
            [&table, &key](const auto& value_c) {
                using T = std::decay_t<decltype(value_c)>;
                if constexpr(std::same_as<T, Scalar>) {
                    std::visit(
                        [&table, &key](const auto& value_s) {
                            using U = std::decay_t<decltype(value_s)>;
                            if constexpr(!std::same_as<U, std::monostate>) {
                                table.emplace(key, convert_for_toml(value_s));
                            }
                        },
                        value_c);
                } else if constexpr(std::same_as<T, Array>) {
                    toml::array array {};
                    std::visit(
                        [&array](const auto& value_a) {
                            using U = std::decay_t<decltype(value_a)>;
                            if constexpr(!std::same_as<U, std::monostate>) {
                                array.reserve(value_a.size());
                                std::ranges::for_each(value_a,
                                                      [&array](const auto& v) { array.push_back(convert_for_toml(v)); });
                            }
                        },
                        value_c);
                    table.emplace(key, std::move(array));
                } else if constexpr(std::same_as<T, Dictionary>) {
                    // Set key-value pairs recursively
                    table.emplace(key, get_as_toml_table(value_c));
                } else {
                    std::unreachable();
                }
            },
            value);
    }
    return table;
}
// NOLINTEND(misc-no-recursion)
