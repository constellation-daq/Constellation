/**
 * @file
 * @brief Enums functions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <concepts>
#include <ios>
#include <string_view>
#include <type_traits>

#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#include <magic_enum.hpp>
#endif

namespace constellation::utils {

    template <typename E>
        requires std::is_enum_v<E>
    constexpr auto enum_cast(std::underlying_type_t<E> value) noexcept {
        return magic_enum::enum_cast<E>(value);
    }

    template <typename E>
        requires std::is_enum_v<E>
    constexpr auto enum_cast(std::string_view value, bool case_insesitive = true) noexcept {
        if(case_insesitive) {
            return magic_enum::enum_cast<E>(value, magic_enum::case_insensitive);
        }
        return magic_enum::enum_cast<E>(value);
    }

    template <typename E>
        requires std::is_enum_v<E>
    constexpr auto enum_name(E enum_val) noexcept {
        return magic_enum::enum_name<E>(enum_val);
    }

    template <typename E>
        requires std::is_enum_v<E>
    constexpr auto enum_names() noexcept {
        return magic_enum::enum_names<E>();
    }

} // namespace constellation::utils

// Stream operator<< for enums
template <typename S, typename E>
    requires std::derived_from<std::remove_cvref_t<S>, std::ios_base> && std::is_enum_v<E>
inline S&& operator<<(S&& os, E value) {
    os << constellation::utils::enum_name(value);
    return std::forward<S>(os);
}

// Set enum as flag field
#define ENUM_SET_FLAG(ENUM)                                                                                                 \
    template <> struct magic_enum::customize::enum_range<ENUM> {                                                            \
        static constexpr bool is_flags = true;                                                                              \
    }

// Adjust enum range and specify whether to use as flags
#define ENUM_SET_RANGE(ENUM, MIN, MAX)                                                                                      \
    template <> struct magic_enum::customize::enum_range<ENUM> {                                                            \
        static constexpr int min = MIN;                                                                                     \
        static constexpr int max = MAX;                                                                                     \
    }

// Set enum as flag field while defining its range
#define ENUM_SET_FLAGS_RANGE(ENUM, MIN, MAX)                                                                                \
    template <> struct magic_enum::customize::enum_range<ENUM> {                                                            \
        static constexpr int min = MIN;                                                                                     \
        static constexpr int max = MAX;                                                                                     \
        static constexpr bool is_flags = true;                                                                              \
    }
