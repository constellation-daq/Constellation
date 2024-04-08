/**
 * @file
 * @brief FSM class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "FSM.hpp"

#include <any>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include <msgpack.hpp>
#include <zmq.hpp>

#include "constellation/core/logging/log.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/satellite/exceptions.hpp"
#include "constellation/satellite/fsm_definitions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::literals::string_literals;

FSM::~FSM() {
    transitional_thread_.request_stop();
    if(transitional_thread_.joinable()) {
        transitional_thread_.join();
    }
    run_thread_.request_stop();
    if(run_thread_.joinable()) {
        run_thread_.join();
    }
    failure_thread_.request_stop();
    if(failure_thread_.joinable()) {
        failure_thread_.join();
    }
}

FSM::TransitionFunction FSM::findTransitionFunction(Transition transition) {
    // Get transition map for current state (never throws due to FSM design)
    const auto& transition_map = state_transition_map_.at(state_);
    // Find transition
    const auto transition_function_it = transition_map.find(transition);
    if(transition_function_it != transition_map.end()) [[likely]] {
        return transition_function_it->second;
    } else {
        throw FSMError("Transition " + to_string(transition) + " not allowed from " + to_string(state_) + " state");
    }
}

bool FSM::isAllowed(Transition transition) {
    try {
        findTransitionFunction(transition);
    } catch(const FSMError&) {
        return false;
    }
    return true;
}

void FSM::react(Transition transition, TransitionPayload payload) {
    // Find transition
    LOG(logger_, INFO) << "Reacting to transition " << to_string(transition);
    auto transition_function = findTransitionFunction(transition);
    // Execute transition function
    state_ = (this->*transition_function)(std::move(payload));
    LOG(logger_, STATUS) << "New state: " << to_string(state_);
}

bool FSM::reactIfAllowed(Transition transition, TransitionPayload payload) {
    try {
        react(transition, std::move(payload));
    } catch(const FSMError&) {
        LOG(logger_, INFO) << "Skipping transition " << to_string(transition);
        return false;
    }
    return true;
}

std::pair<CSCP1Message::Type, std::string> FSM::reactCommand(TransitionCommand transition_command,
                                                             std::shared_ptr<zmq::message_t> payload) {
    // Cast to normal transition, underlying values are identical
    auto transition = static_cast<Transition>(transition_command);
    LOG(logger_, INFO) << "Reacting to transition " << to_string(transition);
    // Check if command is a valid transition for the current state
    TransitionFunction transition_function {};
    try {
        transition_function = findTransitionFunction(transition);
    } catch(const FSMError& error) {
        LOG(logger_, WARNING) << error.what();
        return {CSCP1Message::Type::INVALID, error.what()};
    }
    // Check if reconfigure command is implemented in case it is requested
    if(transition == Transition::reconfigure && !satellite_->supportsReconfigure()) {
        std::string reconfigure_info {"Transition reconfigure is not implemented by this satellite"};
        LOG(logger_, WARNING) << reconfigure_info;
        return {CSCP1Message::Type::NOTIMPLEMENTED, std::move(reconfigure_info)};
    }
    // Check if payload only in initialize, reconfigure, and start
    auto should_have_payload =
        (transition == Transition::initialize || transition == Transition::reconfigure || transition == Transition::start);
    if(should_have_payload && !payload) {
        std::string payload_info {"Transition " + to_string(transition) + " requires a payload frame"};
        LOG(logger_, WARNING) << payload_info;
        return {CSCP1Message::Type::INCOMPLETE, std::move(payload_info)};
    }
    // If there is a payload, but it is not used add a note in the reply
    const std::string payload_note = (!should_have_payload && payload) ? " (payload frame is ignored)"s : ""s;

    // Try to decode the payload:
    TransitionPayload fsm_payload {};
    try {
        if(payload && !payload->empty()) {
            if(transition == Transition::initialize || transition == Transition::reconfigure) {
                const auto msgpack_payload = msgpack::unpack(utils::to_char_ptr(payload->data()), payload->size());
                fsm_payload = config::Configuration(msgpack_payload->as<Dictionary>());
            } else if(transition == Transition::start) {
                const auto msgpack_payload = msgpack::unpack(utils::to_char_ptr(payload->data()), payload->size());
                fsm_payload = msgpack_payload->as<std::uint32_t>();
            }
        }
    } catch(msgpack::unpack_error& e) {
        std::string payload_info {"Transition " + to_string(transition) + " received invalid payload: " + e.what()};
        LOG(logger_, WARNING) << payload_info;
        return {CSCP1Message::Type::INCOMPLETE, std::move(payload_info)};
    } catch(std::bad_cast&) {
        std::string payload_info {"Transition " + to_string(transition) + " received incorrect payload"};
        LOG(logger_, WARNING) << payload_info;
        return {CSCP1Message::Type::INCOMPLETE, std::move(payload_info)};
    }

    // Execute transition function
    state_ = (this->*transition_function)(std::move(fsm_payload));
    LOG(logger_, STATUS) << "New state: " << to_string(state_);

    // Return that command is being executed
    return {CSCP1Message::Type::SUCCESS, "Transition " + to_string(transition) + " is being initiated" + payload_note};
}

void FSM::interrupt() {
    LOG(logger_, STATUS) << "Interrupting...";
    //  Wait until we are in a steady state
    while(!is_steady(state_)) {
        LOG_ONCE(logger_, DEBUG) << "Waiting for a steady state...";
    }
    // In a steady state, try to react to interrupt
    reactIfAllowed(Transition::interrupt);
    // We could be in interrupting, so wait for steady state
    while(!is_steady(state_)) {
        LOG_ONCE(logger_, DEBUG) << "Waiting for a steady state...";
    }
}

template <typename Func, typename... Args>
Transition call_satellite_function(Satellite* satellite, Func func, Transition success_transition, Args... args) {
    try {
        // Note: we should probably use a packaged task to quit this externally like https://stackoverflow.com/a/56268886
        (satellite->*func)(args...);
        // Finish transition
        return success_transition;
    } catch(...) {
        // something went wrong, go to error state
        return Transition::failure;
    }
}

// NOLINTBEGIN(performance-unnecessary-value-param)

State FSM::initialize(TransitionPayload payload) {
    auto cfg = std::get<Configuration>(payload);
    auto call_wrapper = [this](const std::stop_token& stop_token, const Configuration& config) {
        // We might come from ERROR, so let's stop the failure thread
        this->failure_thread_.request_stop();
        if(this->failure_thread_.joinable()) {
            this->failure_thread_.join();
        }
        // Note: we should quit this externally after some time if a stop was requested, see also `call_satellite_function`

        LOG(logger_, INFO) << "Calling initializing function of satellite...";
        const auto transition = call_satellite_function(
            this->satellite_.get(), &Satellite::initializing, Transition::initialized, stop_token, config);
        this->reactIfAllowed(transition);
    };
    transitional_thread_ = std::jthread(call_wrapper, cfg);
    return State::initializing;
}

State FSM::launch(TransitionPayload /* payload */) {
    auto call_wrapper = [this](const std::stop_token& stop_token) {
        LOG(logger_, INFO) << "Calling launching function of satellite...";
        const auto transition =
            call_satellite_function(this->satellite_.get(), &Satellite::launching, Transition::launched, stop_token);
        this->reactIfAllowed(transition);
    };
    transitional_thread_ = std::jthread(call_wrapper);
    return State::launching;
}

State FSM::land(TransitionPayload /* payload */) {
    auto call_wrapper = [this](const std::stop_token& stop_token) {
        LOG(logger_, INFO) << "Calling landing function of satellite...";
        const auto transition =
            call_satellite_function(this->satellite_.get(), &Satellite::landing, Transition::landed, stop_token);
        this->reactIfAllowed(transition);
    };
    transitional_thread_ = std::jthread(call_wrapper);
    return State::landing;
}

State FSM::reconfigure(TransitionPayload payload) {
    auto cfg = std::get<Configuration>(payload);
    auto call_wrapper = [this](const std::stop_token& stop_token, const Configuration& partial_config) {
        LOG(logger_, INFO) << "Calling reconfiguring function of satellite...";
        const auto transition = call_satellite_function(
            this->satellite_.get(), &Satellite::reconfiguring, Transition::reconfigured, stop_token, partial_config);
        this->reactIfAllowed(transition);
    };
    transitional_thread_ = std::jthread(call_wrapper, cfg);
    return State::reconfiguring;
}

State FSM::start(TransitionPayload payload) {
    auto run = std::get<std::uint32_t>(payload);
    auto call_wrapper = [this](const std::stop_token& stop_token, std::uint32_t run_nr) {
        LOG(logger_, INFO) << "Calling starting function of satellite...";
        const auto transition =
            call_satellite_function(this->satellite_.get(), &Satellite::starting, Transition::started, stop_token, run_nr);
        this->reactIfAllowed(transition);
    };
    transitional_thread_ = std::jthread(call_wrapper, run);
    return State::starting;
}

State FSM::started(TransitionPayload /* payload */) {
    // Start running thread async
    auto call_wrapper = [this](const std::stop_token& stop_token) { satellite_->running(stop_token); };
    run_thread_ = std::jthread(call_wrapper);
    return State::RUN;
}

State FSM::stop(TransitionPayload /* payload */) {
    auto call_wrapper = [this](const std::stop_token& stop_token) {
        // First stop of RUN thread
        this->run_thread_.request_stop();
        if(this->run_thread_.joinable()) {
            this->run_thread_.join();
        }
        // Note: we should quit this externally after some time if a stop was requested, see also `call_satellite_function`

        LOG(logger_, INFO) << "Calling stopping function of satellite...";
        const auto transition =
            call_satellite_function(this->satellite_.get(), &Satellite::stopping, Transition::stopped, stop_token);
        this->reactIfAllowed(transition);
    };
    transitional_thread_ = std::jthread(call_wrapper);
    return State::stopping;
}

State FSM::interrupt(TransitionPayload /* payload */) {
    auto call_wrapper = [this](const std::stop_token& stop_token) {
        LOG(logger_, INFO) << "Calling interrupting function of satellite...";
        const auto transition = call_satellite_function(
            this->satellite_.get(), &Satellite::interrupting, Transition::interrupted, stop_token, this->state_);
        this->reactIfAllowed(transition);
    };
    transitional_thread_ = std::jthread(call_wrapper);
    return State::interrupting;
}

State FSM::failure(TransitionPayload /* payload */) {
    auto call_wrapper = [this](const std::stop_token& stop_token) {
        LOG(logger_, INFO) << "Calling on_failure function of satellite...";
        call_satellite_function(
            this->satellite_.get(), &Satellite::on_failure, Transition::failure, stop_token, this->state_);
        // Note: we do not trigger a success transition as we always go to ERROR state
    };
    failure_thread_ = std::jthread(call_wrapper);
    return State::ERROR;
}

// NOLINTEND(performance-unnecessary-value-param)
