/**
 * @file
 * @brief Thread utilities
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <concepts>
#include <string>
#include <thread>

#ifdef __linux__
#include <pthread.h>
#endif

namespace constellation::utils {

    template <typename T>
        requires std::same_as<T, std::thread> || std::same_as<T, std::jthread>
    inline void set_thread_name([[maybe_unused]] T& thread, [[maybe_unused]] const std::string& name) {
#ifdef __linux__
        pthread_setname_np(thread.native_handle(), name.substr(0, 15).c_str());
#endif
        // TODO(stephn.lachnit): Implement for Windows / MacOS
    }
} // namespace constellation::utils
