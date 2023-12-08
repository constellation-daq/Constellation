/**
 * @file
 * @brief CHIRP API configuration
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <version>

#if (defined _WIN32 && !defined __CYGWIN__)
#if CHIRP_BUILDLIB
#define CHIRP_API __declspec(dllexport)
#else
#define CHIRP_API __declspec(dllimport)
#endif
#else
#if CHIRP_BUILDLIB
#define CHIRP_API __attribute__((__visibility__("default")))
#else
#define CHIRP_API
#endif
#endif

// std::to_underlying
#ifndef __cpp_lib_to_underlying
#include <type_traits>
namespace std {
template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) noexcept {
    return static_cast<typename std::underlying_type<E>::type>(e);
}
}
#endif

// std::unreachable
#ifndef __cpp_lib_unreachable
namespace std{
[[noreturn]] inline void unreachable() {
    #ifdef __GNUC__
    __builtin_unreachable();
    #else
    #ifdef _MSC_VER
    __assume(false);
    #endif
    #endif
}
}
#endif
