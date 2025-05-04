/**
 * @file
 * @brief Implementation of a measurement queue
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MeasurementQueue.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include "constellation/controller/exceptions.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/timers.hpp"

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::chrono_literals;

MeasurementQueue::~MeasurementQueue() {
    // Interrupt the queue if one was running:
    interrupt();

    // Join the queue thread:
    if(queue_thread_.joinable()) {
        queue_thread_.join();
    }
}

void MeasurementQueue::append(Measurement measurement, std::optional<Condition> condition) {
    const std::lock_guard measurement_lock {measurement_mutex_};
    measurements_.emplace(std::move(measurement), condition);
    size_at_start_++;
}

void MeasurementQueue::start() {
    LOG(logger_, DEBUG) << "Requested starting of queue";

    // Already running?
    if(queue_running_) {
        // throw QueueError("Queue already running");
        LOG(logger_, WARNING) << "Queue already running";
        return;
    }

    // We only start when we are in orbit
    if(!controller_.isInState(CSCP::State::ORBIT)) {
        // throw QueueError("Controller not in correct state");
        LOG(logger_, WARNING) << "Not in correct state, controller reports " << controller_.getLowestState();
        return;
    }

    // Join if exists from previous run:
    if(queue_thread_.joinable()) {
        queue_thread_.request_stop();
        queue_thread_.join();
    }

    const std::lock_guard measurement_lock {measurement_mutex_};
    size_at_start_ = measurements_.size();
    queue_thread_ = std::jthread(std::bind_front(&MeasurementQueue::queue_loop, this));
}

void MeasurementQueue::halt() {
    LOG(logger_, DEBUG) << "Requested halting of queue";

    if(!queue_running_) {
        LOG(logger_, DEBUG) << "No queue running";
        return;
    }

    // Stop sender thread
    queue_thread_.request_stop();
}

void MeasurementQueue::interrupt() {
    LOG(logger_, DEBUG) << "Requested interruption of queue";

    if(!queue_running_) {
        LOG(logger_, DEBUG) << "No queue running";
        return;
    }

    // Request a stop to be sure we're not starting a new measurement just now:
    queue_thread_.request_stop();

    // Set the queue to stopped to interrupt current measurement
    queue_running_ = false;
    interrupt_counter_++;
}

void MeasurementQueue::await_state(CSCP::State state) const {
    auto timer = TimeoutTimer(transition_timeout_);
    timer.reset();

    while(!controller_.isInState(state)) {
        if(timer.timeoutReached()) {
            throw QueueError("Timeout out waiting for global state " + to_string(state));
        }

        std::this_thread::sleep_for(100ms);
    }
}

void MeasurementQueue::await_condition(Condition /*condition*/) const {
    auto timer = TimeoutTimer(5s);
    timer.reset();
    while(queue_running_ && !timer.timeoutReached()) {
        std::this_thread::sleep_for(100ms);
    }
}

void MeasurementQueue::check_replies(const std::map<std::string, message::CSCP1Message>& replies) const {
    const auto success = std::ranges::all_of(replies.cbegin(), replies.cend(), [&](const auto& reply) {
        const auto verb = reply.second.getVerb();
        const auto success = (verb.first == CSCP1Message::Type::SUCCESS);
        if(!success) {
            LOG(logger_, WARNING) << "Satellite " << reply.first << " replied with " << verb.first << ": " << verb.second;
        }
        return success;
    });

    if(!success) {
        throw QueueError("Unexpected reply from satellite");
    }
}

void MeasurementQueue::queue_loop(const std::stop_token& stop_token) {
    try {
        std::unique_lock measurement_lock {measurement_mutex_};

        LOG(logger_, STATUS) << "Started measurement queue";
        queue_running_ = true;

        // Loop until either a stop is requested or we run out of measurements:
        while(!stop_token.stop_requested() && !measurements_.empty()) {
            // Start a new measurement:
            const auto [measurement, condition] = measurements_.front();
            measurement_lock.unlock();
            LOG(logger_, STATUS) << "Starting new measurement from queue, " << measurement.size()
                                 << " satellite configurations";

            // Wait for ORBIT state across all
            await_state(CSCP::State::ORBIT);

            // Update constellation - satellites without payload will not receive the command
            LOG(logger_, INFO) << "Reconfiguring satellites";
            const auto reply_reconf = controller_.sendCommands("reconfigure", measurement, false);
            check_replies(reply_reconf);

            // Wait for ORBIT state across all
            await_state(CSCP::State::ORBIT);

            // Start the measurement for all satellites
            LOG(logger_, INFO) << "Starting satellites";
            auto run_identifier = run_identifier_prefix_ + std::to_string(run_sequence_);
            if(interrupt_counter_ > 0) {
                run_identifier += "_retry_" + std::to_string(interrupt_counter_);
            }
            const auto reply_start = controller_.sendCommands("start", run_identifier);
            check_replies(reply_start);

            // Wait for RUN state across all
            await_state(CSCP::State::RUN);

            // Wait for condition to be come true
            await_condition(condition.value_or(default_condition_));

            // Stop the constellation
            LOG(logger_, INFO) << "Stopping satellites";
            const auto reply_stop = controller_.sendCommands("stop");
            check_replies(reply_stop);

            // Wait for ORBIT state across all
            await_state(CSCP::State::ORBIT);

            measurement_lock.lock();
            // Successfully concluded this measurement, pop it - skip if interrupted
            if(queue_running_) {
                measurements_.pop();
                run_sequence_++;
                interrupt_counter_ = 0;
            }
        }
    } catch(const std::exception& error) {
        LOG(logger_, CRITICAL) << "Caught exception in queue thread: " << error.what();
    } catch(...) {
        LOG(logger_, CRITICAL) << "Caught exception in queue thread";
    }

    LOG(logger_, STATUS) << "Queue ended";
    queue_running_ = false;
}
