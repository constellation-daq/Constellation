/**
 * @file
 * @brief Metrics
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Metric.hpp"

#include <chrono>

using namespace constellation::metrics;
using namespace constellation::message;

void MetricTimer::update(const config::Value& value) {
    if(value != Metric::value()) {
        set(value);
        changed_ = true;
    }
}

bool MetricTimer::check(State state) {

    // First check the metric condition to update internals
    if(!condition()) {
        return false;
    }

    // If the metric has not been changed, there is no need to send it again
    if(!changed_) {
        return false;
    }

    // Check if we are supposed to distribute this metric from the current state:
    // Note: empty state list means that it is always distributed.
    if(!states_.empty() && !states_.contains(state)) {
        return false;
    }

    // All checks passed, send metric
    changed_ = false;
    return true;
}

bool TimedMetric::condition() {

    auto duration = Clock::now() - last_trigger_;

    if(duration >= interval_) {
        last_trigger_ += interval_;
        return true;
    }

    last_check_ = Clock::now();
    return false;
}

Clock::time_point TimedMetric::next_trigger() const {
    return last_check_ + interval_;
}

TriggeredMetric::TriggeredMetric(std::string_view unit,
                                 Type type,
                                 std::size_t triggers,
                                 std::initializer_list<constellation::message::State> states,
                                 const config::Value& value)
    : MetricTimer(unit, type, states, value), triggers_(triggers) {
    // We have an initial value, let's log it directly
    if(!std::holds_alternative<std::monostate>(value)) {
        current_triggers_ = triggers_;
    }
}

void TriggeredMetric::update(const config::Value& value) {
    MetricTimer::update(value);
    current_triggers_++;
}

bool TriggeredMetric::condition() {

    if(current_triggers_ >= triggers_) {
        current_triggers_ = 0;
        return true;
    }

    return false;
}
