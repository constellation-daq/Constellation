/**
 * @file
 * @brief Subscriber pool for CMDP telemetry
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <functional>
#include <set>
#include <string>
#include <string_view>

#include "constellation/build.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/listener/CMDPListener.hpp"

namespace constellation::listener {

    class CNSTLN_API StatListener : public CMDPListener {
    public:
        /**
         * @brief Construct telemetry listener
         *
         * @param log_topic Logger topic to be used for this component
         * @param callback Callback function pointer for received messages
         */
        StatListener(std::string_view log_topic, std::function<void(message::CMDP1StatMessage&&)> callback);

        /**
         * @brief Subscribe to a specific metric from all connected hosts
         *
         * This subscribes to `STAT/<metric>`
         *
         * @param metric Telemetry topic to subscribe to
         */
        void subscribeMetric(const std::string& metric);

        /**
         * @brief Unsubscribe from a specific metric from all connected hosts
         *
         * @param metric Telemetry topic to unsubscribe from
         */
        void unsubscribeMetric(const std::string& metric);

        /**
         * @brief Get set of subscribed metrics
         *
         * @return Set with telemetry topics
         */
        std::set<std::string> getMetricSubscriptions();

        /**
         * @brief Subscribe to an extra telemetry topic for a specific host
         *
         * This subscribes to `STAT/<metric>`
         *
         * @param host Canonical name of the host
         * @param metric Telemetry topic to subscribe to
         */
        void subscribeMetric(const std::string& host, const std::string& metric);

        /**
         * @brief Unsubscribe from an extra telemetry topic for a specific host
         *
         * @param host Canonical name of the host
         * @param metric Telemetry topic to subscribe to
         */
        void unsubscribeMetric(const std::string& host, const std::string& metric);

        /**
         * @brief Get subscribed extra metrics for a specific host
         *
         * @return Set with telemetry topics
         */
        std::set<std::string> getMetricSubscriptions(const std::string& host);

    private:
        CNSTLN_LOCAL static std::string_view demangle_topic(std::string_view topic);

        // Hide subscribe/unsubscribe functions from CMDPListener
        // clang-format off
        using CMDPListener::subscribeTopic;
        using CMDPListener::unsubscribeTopic;
        using CMDPListener::multiscribeTopics;
        using CMDPListener::getTopicSubscriptions;
        using CMDPListener::subscribeExtraTopic;
        using CMDPListener::unsubscribeExtraTopic;
        using CMDPListener::multiscribeExtraTopics;
        using CMDPListener::getExtraTopicSubscriptions;
        using CMDPListener::removeExtraTopicSubscriptions;
        // clang-format on
    };

} // namespace constellation::listener
