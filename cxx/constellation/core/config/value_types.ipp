/**
 * @file
 * @brief Inline implementation of configuration value types
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "value_types.hpp" // NOLINT(misc-header-include-cycle)

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <map>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/type.hpp"

namespace constellation::config {

    // --- Helpers ---

    template <typename E>
        requires std::is_enum_v<E>
    E config_enum_cast(std::string_view value) {
        const auto enum_value = utils::enum_cast<E>(value);
        if(!enum_value.has_value()) {
            throw std::invalid_argument("value " + utils::quote(value) + " is not valid, possible values are " +
                                        utils::list_enum_names<E>());
        }
        return enum_value.value();
    }

    template <typename T, typename U> T config_numeric_cast(U value) {
        if(!std::in_range<T>(value)) [[unlikely]] {
            throw std::invalid_argument("value " + utils::quote(utils::to_string(value)) + " is out of range for " +
                                        utils::quote(utils::demangle<T>()));
        }
        return static_cast<T>(value);
    }

    // --- Scalar ---

    template <typename T>
        requires scalar_constructible<T>
    Scalar::Scalar(T value) {
        if constexpr(utils::is_one_of_v<T, ScalarVariant>) {
            emplace<T>(std::move(value));
        } else if constexpr(std::integral<T>) {
            emplace<std::int64_t>(config_numeric_cast<std::int64_t>(value));
        } else if constexpr(std::floating_point<T>) {
            emplace<double>(static_cast<double>(value));
        } else if constexpr(std::convertible_to<T, std::string_view>) {
            emplace<std::string>(std::string(std::string_view(value)));
        } else if constexpr(std::is_enum_v<T>) {
            emplace<std::string>(utils::enum_name(value));
        } else {
            std::unreachable();
        }
    }

    template <typename T>
        requires scalar_constructible<T>
    Scalar& Scalar::operator=(T other) {
        Scalar scalar {std::move(other)};
        this->swap(scalar);
        return *this;
    }

    template <typename T>
        requires scalar_constructible<T>
    bool Scalar::operator==(T other) const {
        return *this == Scalar(std::move(other));
    }

    template <typename T>
        requires scalar<T>
    T Scalar::get() const {
        if constexpr(utils::is_one_of_v<T, ScalarVariant>) {
            return std::get<T>(*this);
        } else if constexpr(std::integral<T>) {
            return config_numeric_cast<T>(std::get<std::int64_t>(*this));
        } else if constexpr(std::floating_point<T>) {
            return static_cast<T>(std::get<double>(*this));
        } else if constexpr(std::same_as<T, std::string_view>) {
            return std::string_view(std::get<std::string>(*this));
        } else if constexpr(std::is_enum_v<T>) {
            return config_enum_cast<T>(std::get<std::string>(*this));
        }
        std::unreachable();
    }

    // --- Array ---

    template <typename T>
        requires scalar_constructible<T>
    Array::Array(std::initializer_list<T> init) : Array(std::vector(init.begin(), init.end())) {}

    template <typename R>
        requires array_constructible<R>
    Array::Array(const R& range) {
        if(std::ranges::empty(range)) {
            // Emplace monostate for empty ranges
            emplace<std::monostate>();
        } else {
            using T = std::ranges::range_value_t<R>;
            if constexpr(utils::is_one_of_v<std::vector<T>, ArrayVariant>) {
                emplace<std::vector<T>>(static_transform<T>(range, [](auto arg) { return arg; }));
            } else if constexpr(std::integral<T>) {
                emplace<std::vector<std::int64_t>>(
                    static_transform<std::int64_t>(range, [](auto arg) { return config_numeric_cast<std::int64_t>(arg); }));
            } else if constexpr(std::floating_point<T>) {
                emplace<std::vector<double>>(
                    static_transform<double>(range, [](auto arg) { return static_cast<double>(arg); }));
            } else if constexpr(std::convertible_to<T, std::string_view>) {
                emplace<std::vector<std::string>>(static_transform<std::string>(
                    range, [](auto&& arg) { return std::string(std::string_view(std::forward<decltype(arg)>(arg))); }));
            } else if constexpr(std::is_enum_v<T>) {
                emplace<std::vector<std::string>>(
                    static_transform<std::string>(range, [](auto arg) { return utils::enum_name(arg); }));
            } else {
                std::unreachable();
            }
        }
    }

    template <typename R>
        requires array_constructible<R>
    Array& Array::operator=(const R& other) {
        Array array {other};
        this->swap(other);
        return *this;
    }

    template <typename R>
        requires array_constructible<R>
    bool Array::operator==(const R& other) const {
        return *this == Array(other);
    }

    template <typename T>
        requires scalar<T>
    std::vector<T> Array::getVector() const {
        // If monostate, return empty vector
        if(std::holds_alternative<std::monostate>(*this)) {
            return {};
        }
        if constexpr(utils::is_one_of_v<std::vector<T>, ArrayVariant>) {
            return std::get<std::vector<T>>(*this);
        } else if constexpr(std::integral<T>) {
            return transform<T, std::int64_t>([](auto arg) { return config_numeric_cast<T>(arg); });
        } else if constexpr(std::floating_point<T>) {
            return transform<T, double>([](auto arg) { return static_cast<T>(arg); });
        } else if constexpr(std::same_as<T, std::string_view>) {
            return transform<T, std::string>([](const auto& arg) { return std::string_view(arg); });
        } else if constexpr(std::is_enum_v<T>) {
            return transform<T, std::string>([](const auto& arg) { return config_enum_cast<T>(arg); });
        }
        std::unreachable();
    }

    template <typename T, typename R, typename F> std::vector<T> Array::static_transform(const R& range, F&& op) {
        std::vector<T> out {};
        if constexpr(std::ranges::sized_range<R>) {
            out.reserve(std::ranges::size(range));
        }
        std::ranges::transform(range, std::back_inserter(out), std::forward<F>(op));
        return out;
    }

    template <typename T, typename U, typename F> std::vector<T> Array::transform(F&& op) const {
        return static_transform<T>(std::get<std::vector<U>>(*this), std::forward<F>(op));
    }

    // --- Dictionary ---

    template <typename T>
        requires(!std::same_as<T, Composite>)
    Dictionary::Dictionary(const std::map<std::string, T>& map) {
        for(const auto& [key, value] : map) {
            emplace(key, Composite(value));
        }
    }

    template <typename T>
        requires(!std::same_as<T, Composite>)
    Dictionary& Dictionary::operator=(const std::map<std::string, T>& other) {
        *this = Dictionary(other);
        return *this;
    }

    template <typename T>
        requires(!std::same_as<T, Composite>)
    bool Dictionary::operator==(const std::map<std::string, T>& other) const {
        return *this == Dictionary(other);
    }

    template <typename T> std::map<std::string, T> Dictionary::getMap() const {
        std::map<std::string, T> out {};
        for(const auto& [key, value] : *this) {
            // Note: value.get<T>() works on compilers newer than GCC 12
            out.emplace(key, value.template get<T>());
        }
        return out;
    }

    // --- Composite ---

    template <typename T>
        requires utils::is_one_of_v<T, CompositeVariant>
    Composite::Composite(T value) : CompositeVariant(std::move(value)) {}

    template <typename T>
        requires composite_constructible<T>
    Composite::Composite(const T& value) {
        if constexpr(std::constructible_from<Scalar, T>) {
            emplace<Scalar>(Scalar(value));
        } else if constexpr(std::constructible_from<Array, T>) {
            emplace<Array>(Array(value));
        } else if constexpr(std::constructible_from<Dictionary, T>) {
            emplace<Dictionary>(Dictionary(value));
        } else {
            std::unreachable();
        }
    }

    template <typename T>
        requires composite_constructible<T>
    Composite& Composite::operator=(const T& other) {
        Composite composite {other};
        this->swap(composite);
        return *this;
    }

    template <typename T>
        requires composite_constructible<T>
    bool Composite::operator==(const T& other) const {
        return *this == Composite(other);
    }

    template <typename T>
        requires(!utils::is_one_of_v<T, CompositeVariant>)
    T Composite::get() const {
        if constexpr(utils::is_std_vector_v<T>) {
            // If std::vector, get via Array
            return std::get<Array>(*this).getVector<typename T::value_type>();
        } else if constexpr(utils::is_std_map_v<T>) {
            // If std::map, get via Dictionary
            return std::get<Dictionary>(*this).getMap<typename T::mapped_type>();
        } else {
            // Otherwise, get via Scalar
            return std::get<Scalar>(*this).get<T>();
        }
    }

    template <typename T>
        requires(utils::is_one_of_v<T, CompositeVariant>)
    const T& Composite::get() const {
        return std::get<T>(*this);
    }

    template <typename T>
        requires(utils::is_one_of_v<T, CompositeVariant>)
    T& Composite::get() {
        return std::get<T>(*this);
    }

    // --- Composite List ---

    template <typename R>
        requires std::ranges::forward_range<R> && std::constructible_from<Composite, std::ranges::range_value_t<R>>
    CompositeList::CompositeList(const R& range) {
        if constexpr(std::ranges::sized_range<R>) {
            reserve(std::ranges::size(range));
        }
        for(const auto& element : range) {
            emplace_back(element);
        }
    }

} // namespace constellation::config
