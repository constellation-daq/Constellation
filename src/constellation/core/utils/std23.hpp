/**
 * @file
 * @brief C++23 library features for C++20
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

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
#include <chrono>
#include <ctime>
#include <iomanip>
#include <time.h>
namespace std {
    template <typename Clock, typename Duration>
    std::ostream& operator<<(std::ostream& stream, const std::chrono::time_point<Clock, Duration>& time_point) {
        const auto tt = Clock::to_time_t(time_point);
        std::tm tm {};
        localtime_r(&tt, &tm); // there is no thread-safe std::locatime
        return stream << std::put_time(&tm, "%F %T.000000000");
    }
} // namespace std
#endif

// NOLINTEND(cert-dcl58-cpp)
