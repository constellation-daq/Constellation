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

#include <cstddef>

namespace constellation {

    inline char* to_char_ptr(std::byte* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<char*>(data);
    }

    inline void* to_void_ptr(std::byte* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<void*>(data);
    }

    inline std::byte* to_byte_ptr(char* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<std::byte*>(data);
    }

    inline std::byte* to_byte_ptr(void* data) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<std::byte*>(data);
    }

} // namespace constellation
