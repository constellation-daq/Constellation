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
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>

#include "constellation/controller/Controller.hpp"
#include "constellation/controller/exceptions.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
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

void MeasurementQueue::TimerCondition::await(std::atomic_bool& running, Controller& controller, Logger& logger) const {
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

MeasurementQueue::MetricCondition::MetricCondition(std::string remote,
                                                   std::string metric,
                                                   config::Value target,
                                                   std::function<bool(config::Value, config::Value)> comparator)
    : remote_(std::move(remote)), metric_(std::move(metric)), target_(target), comparator_(comparator),
      metric_reception_timeout_(std::chrono::seconds(60)) {};

void MeasurementQueue::MetricCondition::await(std::atomic_bool& running, Controller& controller, Logger& logger) const {
    LOG(logger, DEBUG) << "Running until " << remote_ << " reports " << metric_ << " >= " << target_.str();

    std::atomic<bool> condition_satisfied {false};
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    auto stat_listener = StatListener("QUEUE", [&](message::CMDP1StatMessage&& msg) {
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

MeasurementQueue::MeasurementQueue(Controller& controller,
                                   std::string prefix,
                                   std::shared_ptr<Condition> condition,
                                   std::chrono::seconds timeout)
    : logger_("QUEUE"), run_identifier_prefix_(std::move(prefix)), default_condition_(std::move(condition)),
      transition_timeout_(timeout), controller_(controller) {};

MeasurementQueue::~MeasurementQueue() {
    // Interrupt the queue if one was running:
    interrupt();

    // Join the queue thread:
    if(queue_thread_.joinable()) {
        queue_thread_.join();
    }
}

void MeasurementQueue::append(Measurement measurement, std::shared_ptr<Condition> condition) {
    // Check that satellite names are valid canonical names:
    if(!std::ranges::all_of(measurement, [](const auto& elem) { return CSCP::is_valid_canonical_name(elem.first); })) {
        throw QueueError("Measurement contains invalid canonical name");
    }

    const std::lock_guard measurement_lock {measurement_mutex_};
    measurements_.emplace(std::move(measurement), std::move(condition));
    measurements_size_++;
}

double MeasurementQueue::progress() const {
    if(measurements_size_ == 0 && run_sequence_ == 0) {
        return 0.;
    }
    return 1. - (static_cast<double>(measurements_size_) / static_cast<double>(measurements_size_ + run_sequence_));
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

void MeasurementQueue::queue_started() {};
void MeasurementQueue::queue_stopped() {};
void MeasurementQueue::queue_failed() {};
void MeasurementQueue::progress_updated(double /*progress*/) {};

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

void MeasurementQueue::check_replies(const std::map<std::string, message::CSCP1Message>& replies) const {
    const auto success = std::ranges::all_of(replies.cbegin(), replies.cend(), [&](const auto& reply) {
        const auto verb = reply.second.getVerb();
        const auto success = (verb.first == CSCP1Message::Type::SUCCESS);
        if(!success) {
            LOG(logger_, WARNING) << "Satellite " << reply.first << " replied with " << verb.first << ": " << verb.second;
        }
        return success;
    });

    // FIXME too harsh?
    if(!success) {
        throw QueueError("Unexpected reply from satellite");
    }
}

void MeasurementQueue::cache_original_values(Measurement& measurement) {
    // Loop over all satellites in this measurement:
    for(auto& [satellite, cmd_payload] : measurement) {
        LOG(logger_, DEBUG) << "Caching original values for satellite " << satellite;
        if(!original_values_.contains(satellite)) {
            original_values_[satellite] = config::Dictionary {};
        }
        auto& value_cache = std::get<Dictionary>(original_values_[satellite]);

        // Fetch configuration from this satellite:
        const auto& msg = controller_.sendCommand(satellite, "get_config");
        if(msg.getVerb().first != CSCP1Message::Type::SUCCESS) {
            LOG(logger_, DEBUG) << "Could not obtain configuration from satellite " << satellite;
            return;
        }
        const auto config = Dictionary::disassemble(msg.getPayload());

        // Check if the measurement keys are available in the config:
        auto& measurement_dict = std::get<Dictionary>(cmd_payload);
        for(const auto& [key, value] : measurement_dict) {
            // Check that the key exists in the current configuration:
            if(!config.contains(key)) {
                continue;
            }

            // Insert the key if it has not been registered yet, use the original value obtained from the configuration:
            const auto& [it, inserted] = value_cache.try_emplace(key, config.at(key));
            LOG_IF(logger_, INFO, inserted)
                << "Cached original value `" << key << " = " << config.at(key).str() << "` from satellite " << satellite;
        }

        // Add all original values which are not part of the measurement anymore and drop them from the cache
        for(const auto& [key, value] : value_cache) {
            if(measurement_dict.contains(key)) {
                continue;
            }

            // Insert the key if it has not been registered yet:
            const auto& [it, inserted] = measurement_dict.try_emplace(key, value);
            if(inserted) {
                LOG(logger_, INFO) << "Resetting original value of key " << key << " from satellite " << satellite;
                value_cache.erase(key);
            }
        }
    }
}

void MeasurementQueue::queue_loop(const std::stop_token& stop_token) {
    try {
        std::unique_lock measurement_lock {measurement_mutex_};

        LOG(logger_, STATUS) << "Started measurement queue";
        queue_running_ = true;
        queue_started();

        // Loop until either a stop is requested or we run out of measurements:
        while(!stop_token.stop_requested() && !measurements_.empty()) {
            // Start a new measurement:
            auto [measurement, condition] = measurements_.front();
            measurement_lock.unlock();
            LOG(logger_, STATUS) << "Starting new measurement from queue, " << measurement.size()
                                 << " satellite configurations";

            // Wait for ORBIT state across all
            await_state(CSCP::State::ORBIT);

            // Cache current value of the measurement keys and add original value resets:
            cache_original_values(measurement);

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
            if(condition != nullptr) {
                condition->await(queue_running_, controller_, logger_);
            } else {
                default_condition_->await(queue_running_, controller_, logger_);
            }

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
                measurements_size_--;
                run_sequence_++;
                interrupt_counter_ = 0;
            }

            // Report updated progress
            progress_updated(progress());
        }

        // Reset the original values collected during the measurement:
        LOG(logger_, INFO) << "Resetting parameters to pre-scan values";
        const auto reply_reset = controller_.sendCommands("reconfigure", original_values_, false);
        check_replies(reply_reset);
        original_values_.clear();

        // Wait for ORBIT state across all
        await_state(CSCP::State::ORBIT);

    } catch(const std::exception& error) {
        LOG(logger_, CRITICAL) << "Caught exception in queue thread: " << error.what();
        queue_failed();
    } catch(...) {
        LOG(logger_, CRITICAL) << "Caught exception in queue thread";
        queue_failed();
    }

    LOG(logger_, STATUS) << "Queue ended";
    queue_running_ = false;
    queue_stopped();
}
