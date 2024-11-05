/**
 * @file
 * @brief Metrics Manager
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "constellation/build.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string_hash_map.hpp"

namespace constellation::metrics {

    /** Manager for Metrics handling & transmission */
    class MetricsManager {
    public:
        /**
         * @brief MetricManager taking care of emitting metrics messages
         *
         * @param state_callback Callback to fetch the current state from the finite state machine
         */
        CNSTLN_API MetricsManager(std::function<protocol::CSCP::State()> state_callback);

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        MetricsManager(MetricsManager& other) = delete;
        MetricsManager& operator=(MetricsManager other) = delete;
        MetricsManager(MetricsManager&& other) = delete;
        MetricsManager& operator=(MetricsManager&& other) = delete;
        /// @endcond

        CNSTLN_API virtual ~MetricsManager() noexcept;

        /**
         * Register a (manually triggered) metric
         *
         * @param metric Shared pointer to the metric
         */
        CNSTLN_API void registerMetric(std::shared_ptr<Metric> metric);

        /**
         * Register a (manually triggered) metric
         *
         * @param name Unique topic of the metric
         * @param unit Unit of the provided value
         * @param type Type of the metric
         */
        void registerMetric(std::string name, std::string unit, metrics::MetricType type) {
            registerMetric(std::make_shared<metrics::Metric>(std::move(name), std::move(unit), type));
        };

        /**
         * Register a timed metric
         *
         * @param metric Shared pointer to the timed metric
         */
        CNSTLN_API void registerTimedMetric(std::shared_ptr<TimedMetric> metric);

        /**
         * Unregister a previously registered metric from the manager
         *
         * @param name Name of the metric
         */
        CNSTLN_API void unregisterMetric(std::string_view name);

        /**
         * Unregisters all metrics registered in the manager
         *
         * Equivalent to calling `unregisterMetric` for every registered metric.
         */
        CNSTLN_API void unregisterMetrics();

        /**
         * Manually trigger a metric
         *
         * @param name Name of the metric
         * @param value Value of the metric
         */
        CNSTLN_API void triggerMetric(std::string name, config::Value value);

    private:
        /**
         * Main loop listening and responding to incoming CHIRP broadcasts
         *
         * The run loop responds to incoming CHIRP broadcasts with REQUEST type by sending CHIRP broadcasts with OFFER type
         * for all registered services. It also tracks incoming CHIRP broadcasts with OFFER and DEPART type to form the list
         * of discovered services and calls the corresponding discovery callbacks.
         *
         * @param stop_token Token to stop loop via `std::jthread`
         */
        void run(const std::stop_token& stop_token);

        struct TimedMetricEntry {
            std::shared_ptr<TimedMetric> metric;
            std::chrono::steady_clock::time_point last_sent;
        };

    private:
        log::Logger logger_;
        std::function<protocol::CSCP::State()> state_callback_;

        // Contains all metrics, including timed ones
        utils::string_hash_map<std::shared_ptr<Metric>> metrics_;
        std::mutex metrics_mutex_;

        // Only timed metrics for background thread
        utils::string_hash_map<TimedMetricEntry> timed_metrics_;
        std::mutex timed_metrics_mutex_;

        // Queue for manually triggered metrics
        std::queue<std::pair<std::string, config::Value>> triggered_queue_;
        std::mutex triggered_queue_mutex_;
        std::condition_variable cv_;

        std::jthread thread_;
    };
} // namespace constellation::metrics
