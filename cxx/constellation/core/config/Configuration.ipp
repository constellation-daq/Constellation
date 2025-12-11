/**
 * @file
 * @brief Inline implementation of configuration
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "Configuration.hpp" // NOLINT(misc-header-include-cycle)

#include <cctype>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/core/config/exceptions.hpp"
#include "constellation/core/config/value_types.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::config {

    template <typename T> void Section::setDefault(std::string_view key, T&& default_value) const {
        const auto key_lc = utils::transform(key, tolower);
        dictionary_->try_emplace(key_lc, std::forward<T>(default_value));
    }

    template <typename T> T Section::get(std::string_view key) const {
        // Ensure that get<Dictionary> does not work
        if constexpr(std::same_as<T, Dictionary>) {
            throw utils::LogicError("`get<Dictionary>` called, usage of `getSection` required");
        }

        const auto key_lc = utils::transform(key, tolower);
        try {
            const auto composite = dictionary_->at(key_lc);
            const auto value = composite.get<T>();
            mark_used(key_lc);
            return value;
        } catch(const std::out_of_range&) {
            // Requested key has not been found in dictionary
            throw MissingKeyError(prefix_ + utils::to_string(key));
        } catch(const std::bad_variant_access&) {
            // Value held by the dictionary entry could not be cast to desired type
            throw InvalidTypeError(
                prefix_ + utils::to_string(key), dictionary_->at(key_lc).demangle(), utils::demangle<T>());
        } catch(const std::invalid_argument& error) {
            // Value held by the dictionary entry is not valid (e.g. out of range)
            throw InvalidValueError(prefix_ + utils::to_string(key), error.what());
        }
    }

    template <typename T> T Section::get(std::string_view key, T default_value) const {
        setDefault(key, std::move(default_value));
        return get<T>(key);
    }

    template <typename T> std::optional<T> Section::getOptional(std::string_view key) const {
        try {
            return get<T>(key);
        } catch(const MissingKeyError&) {
            return std::nullopt;
        }
    }

    template <typename T> std::vector<T> Section::getArray(std::string_view key) const {
        try {
            // First, try reading as single value
            return {get<T>(key)};
        } catch(const InvalidTypeError&) {
            // Try reading as array
            return get<std::vector<T>>(key);
        }
    }

    template <typename T> std::vector<T> Section::getArray(std::string_view key, std::vector<T> default_value) const {
        setDefault(key, std::move(default_value));
        return getArray<T>(key);
    }

    template <typename T> std::optional<std::vector<T>> Section::getOptionalArray(std::string_view key) const {
        try {
            return getArray<T>(key);
        } catch(const MissingKeyError&) {
            return std::nullopt;
        }
    }

    template <typename T> std::set<T> Section::getSet(std::string_view key) const {
        auto vec = getArray<T>(key);
        return {std::make_move_iterator(vec.begin()), std::make_move_iterator(vec.end())};
    }

    template <typename T> std::set<T> Section::getSet(std::string_view key, const std::set<T>& default_value) const {
        setDefault(key, std::vector<T>({default_value.cbegin(), default_value.cend()}));
        return getSet<T>(key);
    }

    template <typename T> std::optional<std::set<T>> Section::getOptionalSet(std::string_view key) const {
        try {
            return getSet<T>(key);
        } catch(const MissingKeyError&) {
            return std::nullopt;
        }
    }

} // namespace constellation::config
