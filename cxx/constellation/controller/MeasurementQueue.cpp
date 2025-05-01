/**
 * @file
 * @brief Implementation of a measurement queue
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MeasurementQueue.hpp"

#include <thread>

#include "constellation/controller/exceptions.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::protocol;

MeasurementQueue::~MeasurementQueue() {
    // Interrupt the queue if one was running:
    interrupt();
}

void MeasurementQueue::append(Measurement measurement) {
    // FIXME lock queue

    measurements_.push(std::move(measurement));
    size_at_start_++;
}

void MeasurementQueue::start() {
    LOG(logger_, DEBUG) << "Requested starting of queue";

    // We only start when we are in orbit
    if(!controller_.isInState(CSCP::State::ORBIT)) {
        // throw QueueError("Controller not in correct state");
        LOG(logger_, WARNING) << "Not in correct state, controller reports " << controller_.getLowestState();
        return;
    }

    // Already running?
    if(queue_thread_.joinable()) {
        // throw QueueError("Queue already running");
        LOG(logger_, WARNING) << "Queue already running";
        return;
    }

    // FIXME lock queue
    size_at_start_ = measurements_.size();
    queue_thread_ = std::jthread(std::bind_front(&MeasurementQueue::queue_loop, this));
}

void MeasurementQueue::halt() {
    LOG(logger_, DEBUG) << "Requested halting of queue";

    if(!queue_thread_.joinable()) {
        LOG(logger_, DEBUG) << "No queue running";
        return;
    }

    // Stop sender thread
    queue_thread_.request_stop();
    queue_thread_.join();
}

void MeasurementQueue::interrupt() {
    LOG(logger_, DEBUG) << "Requested interruption of queue";

    // FIXME check if running?
    if(!queue_thread_.joinable()) {
        LOG(logger_, DEBUG) << "No queue running";
        return;
    }

    // Request a stop to be sure we're not starting a new measurement just now:
    queue_thread_.request_stop();

    // Send a stop to the controller if the current state is not already ORBIT
    controller_.sendCommands("stop");

    // Join the queue thread:
    queue_thread_.join();
}

void MeasurementQueue::queue_loop(const std::stop_token& stop_token) {
    // FIXME lock queue
    LOG(logger_, STATUS) << "Started measurement queue";

    // Loop until either a stop is requested or we run out of measurements:
    while(!stop_token.stop_requested() && !measurements_.empty()) {
        // Start a new measurement:
        const auto measurement = measurements_.front();
        LOG(logger_, STATUS) << "Starting new measurement from queue, " << measurement.size() << " satellite configurations";

        // FIXME ensure state ORBIT

        // Update constellation - satellites without payload will not receive the command
        LOG(logger_, INFO) << "Reconfiguring satellites";
        const auto reply_reconf = controller_.sendCommands("reconfigure", measurement, false);

        // Check all responses, abort if something is fishy

        // Wait for ORBIT state across all

        // Start the measurement for all satellites
        LOG(logger_, INFO) << "Starting satellites";
        const auto reply_start = controller_.sendCommands("start");

        // Wait for RUN state across all

        // Wait for condition to be come true

        // Stop the constellation
        LOG(logger_, INFO) << "Stopping satellites";
        const auto reply_stop = controller_.sendCommands("stop");

        // Wait for ORBIT state across all

        // Successfully concluded this measurement, pop it:
        measurements_.pop();
    }

    LOG(logger_, STATUS) << "Queue ended";
}
