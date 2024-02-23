/**
 * @file
 * @brief Compatibility casts for std::byte
 *
 * Note: we should send MRs to msgpackc-cxx and cppzmq to support std::byte* instead
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

#include <magic_enum.hpp>

namespace constellation::utils {

    template <typename T> inline const char* to_char_ptr(T* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<const char*>(data);
    }

    template <typename T> inline const void* to_void_ptr(T* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<const void*>(data);
    }

    template <typename T> inline const std::byte* to_byte_ptr(T* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<const std::byte*>(data);
    }

    template <typename T>
        requires std::same_as<T, std::string_view>
    inline std::string to_string(T string_view) {
        return {string_view.data(), string_view.size()};
    }

    template <typename E>
        requires std::is_enum_v<E>
    inline std::string to_string(E enum_val) {
        return to_string(magic_enum::enum_name<E>(enum_val));
    }

} // namespace constellation::utils
