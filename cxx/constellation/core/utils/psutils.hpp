/**
 * @file
 * @brief Utilities for system information
 *
 * @copyright Copyright (c) 2026 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>

#include "constellation/build.hpp"

namespace constellation::utils {

    /**
     * @brief Get current system load average
     * @details Helper to provide the CPU load average over the past 1 minute. This helper returns a relative value that is
     *          calculated as the POSIX load average divided by the thread concurrency of the system and normalized to 100%.
     *          On Windows, the current active CPU count divided by the thread concurrency is returned as a proxy.
     * @return CPU load average in percent
     */
    CNSTLN_API double get_cpu_load_average();

    /**
     * @brief Get the currently available memory
     * @details Returns the available system memory in units of MiB.
     * @return Available memory in MiB.
     */
    CNSTLN_API std::uint64_t get_available_memory();

}; // namespace constellation::utils
