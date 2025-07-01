/**
 * @file
 * @brief Implementation of a measurement condition
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MeasurementCondition.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <utility>

#include "constellation/controller/Controller.hpp"
#include "constellation/controller/exceptions.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/listener/StatListener.hpp"

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::listener;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::chrono_literals;

void TimerCondition::await(std::atomic_bool& running, Controller& controller, Logger& logger) const {
    // Timed condition, start timer and wait for timeout
    LOG(logger, DEBUG) << "Starting condition timer with " << duration_;
    auto timer = TimeoutTimer(duration_);
    timer.reset();
    while(running && !timer.timeoutReached()) {
        if(controller.hasAnyErrorState()) {
            throw QueueError("Aborting queue processing, detected issue");
        }
        std::this_thread::sleep_for(100ms);
    }
}

std::string TimerCondition::str() const {
    std::string msg = "Run for ";
    msg += to_string(duration_);
    return msg;
}

MetricCondition::MetricCondition(std::string remote,
                                 std::string metric,
                                 config::Value target,
                                 std::function<bool(config::Value, config::Value)> comparator,
                                 std::string comp_name)
    : remote_(std::move(remote)), metric_(std::move(metric)), target_(std::move(target)), comparator_(std::move(comparator)),
      comparator_str_(std::move(comp_name)), metric_reception_timeout_(std::chrono::seconds(60)) {};

void MetricCondition::await(std::atomic_bool& running, Controller& controller, Logger& logger) const {
    LOG(logger, DEBUG) << "Running until " << remote_ << " reports " << metric_ << " " << comparator_str_ << " "
                       << target_.str();

    std::atomic<bool> condition_satisfied {false};
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    auto stat_listener = StatListener("MNTR", [&](CMDP1StatMessage&& msg) {
        // FIXME case-insensitive
        if(msg.getHeader().getSender() != remote_) {
            return;
        }

        const auto& metric_value = msg.getMetric();
        if(metric_value.getMetric()->name() != metric_) {
            return;
        }

        if(!comparator_(metric_value.getValue(), target_)) {
            return;
        }

        condition_satisfied = true;
    });

    // Start the telemetry receiver pool
    stat_listener.startPool();

    // Subscribe to topic:
    stat_listener.subscribeMetric(remote_, metric_);

    // Timeout for metric to have been registered:
    auto metric_timer = TimeoutTimer(metric_reception_timeout_);
    metric_timer.reset();
    bool metric_seen = false;

    // Wait for condition to be met:
    while(running && !condition_satisfied) {
        // Check for error states in the constellation
        if(controller.hasAnyErrorState()) {
            throw QueueError("Aborting queue processing, detected issue");
        }

        if(!metric_seen) {
            const auto topics = stat_listener.getAvailableTopics(remote_);
            metric_seen = topics.contains(metric_);

            // After timeout, break if the metric has not been registered:
            if(metric_timer.timeoutReached() && !metric_seen) {
                std::string msg = "Requested condition metric ";
                msg += metric_;
                msg += " was not registered and never received from satellite ";
                msg += remote_;
                throw QueueError(std::move(msg));
            }
        }

        std::this_thread::sleep_for(100ms);
    }

    stat_listener.unsubscribeMetric(remote_, metric_);
    stat_listener.stopPool();
}

std::string MetricCondition::str() const {
    std::string msg = "Run until ";
    msg += remote_;
    msg += " reports ";
    msg += metric_;
    msg += " ";
    msg += comparator_str_;
    msg += " ";
    msg += target_.str();
    return msg;
}
