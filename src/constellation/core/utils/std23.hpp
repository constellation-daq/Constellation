/**
 * @file
 * @brief C++23 library features for C++20
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <iomanip>
#include <version>

// NOLINTBEGIN(cert-dcl58-cpp)

// std::to_underlying
#ifndef __cpp_lib_to_underlying
#include <type_traits>
namespace std {
    template <typename E> constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept {
        return static_cast<typename std::underlying_type<E>::type>(e);
    }
} // namespace std
#endif

// std::unreachable
#ifndef __cpp_lib_unreachable
namespace std {
    [[noreturn]] inline void unreachable() {
#ifdef __GNUC__
        __builtin_unreachable();
#endif
    }
} // namespace std
#endif

#ifndef __cpp_lib_format
namespace std {
    template <typename Clock, typename Duration>
    std::ostream& operator<<(std::ostream& stream, const std::chrono::time_point<Clock, Duration>& time_point) {
        const time_t time = Clock::to_time_t(time_point);
#if __GNUC__ > 4 || ((__GNUC__ == 4) && __GNUC_MINOR__ > 8 && __GNUC_REVISION__ > 1)
        struct tm tm;
        localtime_r(&time, &tm);
        return stream << std::put_time(&tm, "%c");
#else
        char buffer[26];
        ctime_r(&time, buffer);
        buffer[24] = '\0';
        return stream << buffer;
#endif
    }
} // namespace std
#endif

// NOLINTEND(cert-dcl58-cpp)
