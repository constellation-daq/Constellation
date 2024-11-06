/**
 * @file
 * @brief Stat macros
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic> // IWYU pragma: keep
#include <chrono> // IWYU pragma: keep

#include "constellation/core/metrics/MetricsManager.hpp" // IWYU pragma: keep

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/// @cond doxygen_suppress

// See log.hpp
#define STAT_CONCAT(x, y) x##y
#define STAT_CONCAT_NESTED(x, y) STAT_CONCAT(x, y)
#define STAT_VAR STAT_CONCAT_NESTED(STAT_VAR_L, __LINE__)
#define STAT_VAR2 STAT_CONCAT_NESTED(STAT_VAR2_L, __LINE__)

#define STAT_METRICS_MANAGER constellation::metrics::MetricsManager::getInstance()

/// @endcond

#define STAT(name, value)                                                                                                   \
    if(STAT_METRICS_MANAGER.shouldStat(name))                                                                               \
    STAT_METRICS_MANAGER.triggerMetric(name, value)

#define STAT_IF(name, value, condition)                                                                                     \
    if(STAT_METRICS_MANAGER.shouldStat(name) && (condition))                                                                \
    STAT_METRICS_MANAGER.triggerMetric(name, value)

#define STAT_NTH(name, value, count)                                                                                        \
    do {                                                                                                                    \
        static thread_local std::atomic_size_t STAT_VAR {0};                                                                \
        if(STAT_VAR++ % (count) == 0 && STAT_METRICS_MANAGER.shouldStat(name)) {                                            \
            STAT_METRICS_MANAGER.triggerMetric(name, value);                                                                \
        }                                                                                                                   \
    } while(0)

#define STAT_T(name, value, interval)                                                                                       \
    do {                                                                                                                    \
        static thread_local std::atomic<std::chrono::steady_clock::time_point> STAT_VAR {};                                 \
        const auto STAT_VAR2 = std::chrono::steady_clock::now();                                                            \
        if(STAT_VAR2 - STAT_VAR.load() > (interval) && STAT_METRICS_MANAGER.shouldStat(name)) {                             \
            STAT_VAR.store(STAT_VAR2);                                                                                      \
            STAT_METRICS_MANAGER.triggerMetric(name, value);                                                                \
        }                                                                                                                   \
    } while(0)

// NOLINTEND(cppcoreguidelines-macro-usage)
