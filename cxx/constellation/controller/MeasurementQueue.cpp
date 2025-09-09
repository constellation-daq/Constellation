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
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stop_token>
#include <string>
#include <thread>
#include <tuple>
#include <utility>

#include "constellation/controller/Controller.hpp"
#include "constellation/controller/exceptions.hpp"
#include "constellation/controller/MeasurementCondition.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::chrono_literals;

MeasurementQueue::MeasurementQueue(Controller& controller, std::chrono::seconds timeout)
    : default_condition_(std::make_shared<TimerCondition>(60min)), logger_("CTRL"), run_identifier_prefix_("queue_run_"),
      transition_timeout_(timeout), controller_(controller) {};

MeasurementQueue::~MeasurementQueue() {
    // Interrupt the queue if one was running:
    interrupt();

    // Join the queue thread:
    if(queue_thread_.joinable()) {
        queue_thread_.join();
    }
}

void MeasurementQueue::setPrefix(std::string prefix) {
    run_identifier_prefix_ = std::move(prefix);
}

void MeasurementQueue::setDefaultCondition(std::shared_ptr<MeasurementCondition> condition) {
    // Lock the measurements mutex since the default condition might be used when appending
    const std::lock_guard measurement_lock {measurement_mutex_};

    default_condition_ = std::move(condition);
}

void MeasurementQueue::append(Measurement measurement, std::shared_ptr<MeasurementCondition> condition) {
    // Check that satellite names are valid canonical names:
    if(!std::ranges::all_of(measurement, [](const auto& elem) { return CSCP::is_valid_canonical_name(elem.first); })) {
        throw QueueError("Measurement contains invalid canonical name");
    }

    // Check if all mentioned satellites are present and implement reconfiguration:
    for(const auto& [sat, cfg] : measurement) {
        if(!controller_.hasConnection(sat)) {
            throw QueueError("Satellite " + sat + " is unknown to controller");
        }

        if(!controller_.getConnectionCommands(sat).contains("reconfigure")) {
            throw QueueError("Satellite " + sat + " does not support reconfiguration but has queue parameter");
        }
    }

    std::unique_lock measurement_lock {measurement_mutex_};

    // Use the current default condition of no measurement-specific is provided:
    measurements_.emplace_back(std::move(measurement), (condition == nullptr ? default_condition_ : std::move(condition)));
    measurements_size_++;
    const auto [progress_current, progress_total] = load_progress();
    measurement_lock.unlock();

    // Report updated progress
    progress_updated(progress_current, progress_total);
    queue_state_changed(queue_running_ ? State::RUNNING : State::IDLE, "Added measurement");
}

void MeasurementQueue::clear() {
    std::unique_lock measurement_lock {measurement_mutex_};

    if(measurements_.empty()) {
        return;
    }

    // Get current measurement
    auto current_measurement = std::move(measurements_.front());

    // Clear queue
    measurements_.clear();

    // If running, emplace back current measurement:
    if(queue_running_) {
        measurements_.emplace_back(std::move(current_measurement));
    }

    // Reset the sequence counter:
    run_sequence_ = 0;
    measurements_size_ = measurements_.size();
    const auto [progress_current, progress_total] = load_progress();
    measurement_lock.unlock();

    // Update progress and report:
    progress_updated(progress_current, progress_total);

    if(!queue_running_) {
        queue_state_changed(measurements_size_ == 0 ? State::FINISHED : State::IDLE, "Queue cleared");
    }
}

double MeasurementQueue::progress() const {
    const auto run_sequence = run_sequence_.load();
    const auto measurements_size = measurements_size_.load();

    if(measurements_size == 0 && run_sequence == 0) {
        return 0.;
    }
    return static_cast<double>(run_sequence) / static_cast<double>(measurements_size + run_sequence);
}

void MeasurementQueue::start() {
    LOG(logger_, DEBUG) << "Requested starting of queue";

    // Already running?
    if(queue_running_) {
        LOG(logger_, WARNING) << "Queue already running";
        return;
    }

    // We only start when we are in orbit
    if(!controller_.isInState(CSCP::State::ORBIT)) {
        LOG(logger_, WARNING) << "Not in correct state, controller reports " << controller_.getLowestState();
        return;
    }

    // Join if exists from previous run:
    if(queue_thread_.joinable()) {
        queue_thread_.request_stop();
        queue_thread_.join();
    }

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

void MeasurementQueue::queue_state_changed(State /*queue_state*/, std::string_view /*reason*/) {};
void MeasurementQueue::measurement_concluded() {};
void MeasurementQueue::progress_updated(std::size_t /*current*/, std::size_t /*total*/) {};

std::map<std::string, std::chrono::system_clock::time_point>
MeasurementQueue::get_last_state_change(const Measurement& measurement) const {
    std::set<std::string> satellites {};
    std::ranges::for_each(measurement, [&](const auto& p) { satellites.emplace(p.first); });
    return controller_.getLastStateChange(satellites);
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

void MeasurementQueue::cache_original_values(Measurement& measurement) {
    // Loop over all satellites in this measurement:
    for(auto& [satellite, cmd_payload] : measurement) {
        LOG(logger_, DEBUG) << "Caching original values for satellite " << satellite;
        if(!original_values_.contains(satellite)) {
            original_values_[satellite] = config::Dictionary {};
        }
        auto& value_cache = std::get<Dictionary>(original_values_[satellite]);

        // Fetch configuration from this satellite:
        const auto& message = controller_.sendCommand(satellite, "get_config");
        if(message.getVerb().first != CSCP1Message::Type::SUCCESS) {
            std::string msg = "Could not obtain configuration from satellite ";
            msg += satellite;
            msg += ", ";
            msg += to_string(message.getVerb().second);
            LOG(logger_, CRITICAL) << msg;
            throw QueueError(msg);
        }
        const auto config = Dictionary::disassemble(message.getPayload());

        // Check if the measurement keys are available in the config:
        auto& measurement_dict = std::get<Dictionary>(cmd_payload);
        for(const auto& [key, value] : measurement_dict) {
            // Check that the key exists in the current configuration:
            if(!config.contains(key)) {
                LOG(logger_, WARNING) << "Parameter " << key << " does not exist in configuration of satellite " << satellite
                                      << ", cannot reset original value after queue";
                continue;
            }

            // Insert the key if it has not been registered yet, use the original value obtained from the configuration:
            const auto& [it, inserted] = value_cache.try_emplace(key, config.at(key));
            LOG_IF(logger_, INFO, inserted)
                << "Cached original value " << quote(key + " = " + config.at(key).str()) << " from satellite " << satellite;
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

std::pair<std::size_t, std::size_t> MeasurementQueue::load_progress() const {
    const auto run_sequence = run_sequence_.load();
    const auto measurements_size = measurements_size_.load();
    return {run_sequence, measurements_size + run_sequence};
}

void MeasurementQueue::queue_loop(const std::stop_token& stop_token) {
    try {
        std::unique_lock measurement_lock {measurement_mutex_, std::defer_lock};
        std::once_flag queue_started_flag {};

        // Loop until either a stop is requested or we run out of measurements:
        while(!stop_token.stop_requested() && !measurements_.empty()) {
            // Notify that queue has been started
            std::call_once(queue_started_flag, [this]() {
                LOG(logger_, STATUS) << "Started measurement queue";
                queue_running_ = true;
                queue_state_changed(State::RUNNING, "Started measurement queue");
            });

            // Start a new measurement:
            measurement_lock.lock();
            auto [measurement, condition] = measurements_.front();
            measurement_lock.unlock();
            LOG(logger_, STATUS) << "Starting new measurement from queue, " << measurement.size()
                                 << " satellite configurations";

            // Wait for ORBIT state across all
            controller_.awaitState(CSCP::State::ORBIT, transition_timeout_);

            // Cache current value of the measurement keys and add original value resets:
            cache_original_values(measurement);

            // Update constellation - satellites without payload will not receive the command
            LOG(logger_, INFO) << "Reconfiguring satellites";
            for(const auto& [sat, cfg] : measurement) {
                LOG(logger_, DEBUG) << "Parameters for " << sat << ":";
                for(const auto& [k, v] : std::get<Dictionary>(cfg)) {
                    LOG(logger_, DEBUG) << "\t" << k << " = " << v.str();
                }
            }

            // Get when state was changed before reconfigure command
            auto last_state_change_before_reconf = get_last_state_change(measurement);

            // Send reconfigure command
            const auto reply_reconf = controller_.sendCommands("reconfigure", measurement, false);
            check_replies(reply_reconf);

            // Await ORBIT state while ensuring the states have changed
            controller_.awaitState(CSCP::State::ORBIT, transition_timeout_, std::move(last_state_change_before_reconf));

            // Start the measurement for all satellites
            LOG(logger_, INFO) << "Starting satellites";
            auto run_identifier = run_identifier_prefix_ + std::to_string(run_sequence_);
            if(interrupt_counter_ > 0) {
                run_identifier += "_retry_" + std::to_string(interrupt_counter_);
            }
            const auto reply_start = controller_.sendCommands("start", run_identifier);
            check_replies(reply_start);

            // Wait for RUN state across all
            controller_.awaitState(CSCP::State::RUN, transition_timeout_);

            // Wait for condition to be come true
            condition->await(queue_running_, controller_, logger_);

            // Stop the constellation
            LOG(logger_, INFO) << "Stopping satellites";
            const auto reply_stop = controller_.sendCommands("stop");
            check_replies(reply_stop);

            // Wait for ORBIT state across all
            controller_.awaitState(CSCP::State::ORBIT, transition_timeout_);

            measurement_lock.lock();
            // Successfully concluded this measurement, pop it - skip if interrupted
            if(queue_running_) {
                measurements_.pop_front();
                measurement_concluded();
                measurements_size_--;
                run_sequence_++;
                interrupt_counter_ = 0;
            }
            const auto [progress_current, progress_total] = load_progress();
            measurement_lock.unlock();

            // Report updated progress
            progress_updated(progress_current, progress_total);
        }

        // Reset the original values collected during the measurement:
        LOG(logger_, INFO) << "Resetting parameters to pre-scan values";
        auto last_state_change_before_reconf = get_last_state_change(original_values_);
        const auto reply_reset = controller_.sendCommands("reconfigure", original_values_, false);
        check_replies(reply_reset);
        original_values_.clear();

        // Wait for ORBIT state across all while ensuring the states have changed
        controller_.awaitState(CSCP::State::ORBIT, transition_timeout_, std::move(last_state_change_before_reconf));

        LOG(logger_, STATUS) << "Queue ended";
        queue_running_ = false;
        queue_state_changed(measurements_size_ == 0 ? State::FINISHED : State::IDLE, "Queue ended");
    } catch(const std::exception& error) {
        LOG(logger_, CRITICAL) << "Caught exception in queue thread: " << error.what();
        queue_running_ = false;
        queue_state_changed(State::FAILED, error.what());
    } catch(...) {
        LOG(logger_, CRITICAL) << "Caught exception in queue thread";
        queue_running_ = false;
        queue_state_changed(State::FAILED, "Unknown exception");
    }
}
