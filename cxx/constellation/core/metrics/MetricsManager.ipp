/**
 * @file
 * @brief Metrics manager template members
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "MetricsManager.hpp" // NOLINT(misc-header-include-cycle)

#include <chrono>
#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "constellation/core/config/value_types.hpp"
#include "constellation/core/metrics/exceptions.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/utils/type.hpp"

namespace constellation::metrics {

    inline void MetricsManager::registerMetric(std::string name, std::string unit, std::string description) {
        registerMetric(std::make_shared<metrics::Metric>(std::move(name), std::move(unit), std::move(description)));
    };

    template <typename C>
        requires std::invocable<C>
    void MetricsManager::registerTimedMetric(std::string name,
                                             std::string unit,
                                             std::string description,
                                             std::chrono::steady_clock::duration interval,
                                             C value_callback) {
        std::function<std::optional<config::Scalar>()> value_callback_cast =
            [name, value_callback = std::move(value_callback)]() mutable -> std::optional<config::Scalar> {
            using R = std::invoke_result_t<C>;
            // If optional, wrap the value to std::optional<config::Value>
            if constexpr(utils::is_specialization_of_v<R, std::optional>) {
                auto value = value_callback();
                if(value.has_value()) {
                    return config::Scalar(value.value());
                }
                // Forward empty optional
                return std::nullopt;
            } else {
                // If not optional, set directly
                return config::Scalar(value_callback());
            }
        };
        registerTimedMetric(std::make_shared<TimedMetric>(
            std::move(name), std::move(unit), std::move(description), interval, std::move(value_callback_cast)));
    }

    template <typename T> void MetricsManager::triggerMetric(std::string name, const T& value) {
        // Only emplace name and value, do the lookup in the run thread
        std::unique_lock triggered_queue_lock {triggered_queue_mutex_};
        triggered_queue_.emplace(std::move(name), config::Scalar(value));
        triggered_queue_lock.unlock();
        cv_.notify_one();
    }

} // namespace constellation::metrics
