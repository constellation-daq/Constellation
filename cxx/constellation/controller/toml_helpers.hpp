/**
 * @file
 * @brief Configuration TOML helpers
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <string>

#include <toml++/toml.hpp>

#include "constellation/core/config/value_types.hpp"

namespace constellation::controller {

    std::chrono::system_clock::time_point from_toml_time(const toml::time& toml_time);

    toml::time to_toml_time(const std::chrono::system_clock::time_point& system_time);

    template <typename T, typename F> config::Array convert_toml_array(const toml::array& array, F op);

    config::Dictionary parse_toml_table(const std::string& key, const toml::table& table);

    config::Composite parse_toml_value(const std::string& key, auto&& value);

    toml::table get_as_toml_table(const config::Dictionary& dictionary);

} // namespace constellation::controller

// Include template members
#include "toml_helpers.ipp" // IWYU pragma: keep
