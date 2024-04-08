/**
 * @file
 * @brief Dictionary type with serialization functions for MessagePack
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <magic_enum.hpp>

#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::config {

    template <typename T> T Value::get() const {

        // Value is directly held by variant:
        if constexpr(is_one_of<T, value_t>()) {
            return std::get<T>(*this);
        } else if constexpr(std::is_arithmetic_v<T>) {
            if(std::holds_alternative<std::int64_t>(*this)) {
                return static_cast<T>(std::get<std::int64_t>(*this));
            } else {
                return static_cast<T>(std::get<double>(*this));
            }
        } else if constexpr(std::is_enum_v<T>) {
            const auto str = std::get<std::string>(*this);
            const auto enum_val = magic_enum::enum_cast<T>(utils::transform(str, ::toupper));

            if(!enum_val.has_value()) {
                throw std::invalid_argument("possible values are " + utils::list_enum_names<T>());
            }

            return enum_val.value();
        } else if constexpr(is_vector_v<T>) {
            using U = typename T::value_type;
            if constexpr(std::is_arithmetic_v<U>) {
                if(std::holds_alternative<std::vector<std::int64_t>>(*this)) {
                    const auto vec = std::get<std::vector<std::int64_t>>(*this);
                    return T(vec.begin(), vec.end());
                } else {
                    const auto vec = std::get<std::vector<double>>(*this);
                    return T(vec.begin(), vec.end());
                }
            } else if constexpr(std::is_enum_v<U>) {
                const auto vec = std::get<std::vector<std::string>>(*this);
                T result {};
                result.reserve(vec.size());

                std::for_each(vec.begin(), vec.end(), [&](const auto& str) {
                    const auto enum_val = magic_enum::enum_cast<T>(utils::transform(str, ::toupper));
                    if(!enum_val.has_value()) {
                        throw std::invalid_argument("possible values are " + utils::list_enum_names<T>());
                    }
                    result.emplace_back(enum_val.value());
                });
                return result;
            } else {
                // throw
            }
        } else {
            throw std::bad_variant_access();
        }
    }
} // namespace constellation::config
