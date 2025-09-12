/**
 * @file
 * @brief Telemetry listener implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "StatListener.hpp"

#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/listener/CMDPListener.hpp"

using namespace constellation::listener;
using namespace constellation::message;
using namespace constellation::utils;

StatListener::StatListener(std::string_view log_topic, std::function<void(CMDP1StatMessage&&)> callback)
    : CMDPListener(log_topic,
                   [callback = std::move(callback)](CMDP1Message&& msg) { callback(CMDP1StatMessage(std::move(msg))); }) {
    // Subscribe to notifications:
    CMDPListener::subscribeTopic("STAT?");
}

std::string_view StatListener::demangle_topic(std::string_view topic) {
    return topic.substr(5);
}

void StatListener::subscribeMetric(const std::string& metric) {
    LOG(BasePoolT::pool_logger_, DEBUG) << "Subscribing to telemetry topic " << quote(metric);
    CMDPListener::subscribeTopic("STAT/" + metric);
}

void StatListener::unsubscribeMetric(const std::string& metric) {
    LOG(BasePoolT::pool_logger_, DEBUG) << "Unsubscribing from telemetry topic " << quote(metric);
    CMDPListener::unsubscribeTopic("STAT/" + metric);
}

std::set<std::string> StatListener::getMetricSubscriptions() {
    std::set<std::string> metric_subscriptions {};
    for(const std::string_view topic : CMDPListener::getTopicSubscriptions()) {
        // Ignore notification
        if(topic == "STAT?") {
            continue;
        }
        metric_subscriptions.emplace(demangle_topic(topic));
    }
    return metric_subscriptions;
}

void StatListener::subscribeMetric(const std::string& host, const std::string& metric) {
    LOG(BasePoolT::pool_logger_, DEBUG) << "Subscribing to extra telemetry topic " << quote(metric) << " for host " << host;
    CMDPListener::subscribeExtraTopic(host, "STAT/" + metric);
}

void StatListener::unsubscribeMetric(const std::string& host, const std::string& metric) {
    LOG(BasePoolT::pool_logger_, DEBUG) << "Unsubscribing from extra telemetry topic " << quote(metric) << " for host "
                                        << host;
    CMDPListener::unsubscribeExtraTopic(host, "STAT/" + metric);
}

std::set<std::string> StatListener::getMetricSubscriptions(const std::string& host) {
    std::set<std::string> metric_subscriptions {};
    for(const std::string_view topic : CMDPListener::getExtraTopicSubscriptions(host)) {
        // Ignore notification
        if(topic == "STAT?") {
            continue;
        }
        metric_subscriptions.emplace(demangle_topic(topic));
    }
    return metric_subscriptions;
}
