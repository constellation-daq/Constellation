/**
 * @file
 * @brief FSM class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "FSM.hpp"

#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <iomanip>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <msgpack.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/BaseSatellite.hpp"
#include "constellation/satellite/exceptions.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::string_literals;

FSM::~FSM() {
    terminate();
}

FSM::TransitionFunction FSM::find_transition_function(Transition transition) const {
    // Get transition map for current state (never throws due to FSM design)
    const auto& transition_map = state_transition_map_.at(state_.load());
    // Find transition
    const auto transition_function_it = transition_map.find(transition);
    if(transition_function_it != transition_map.end()) [[likely]] {
        return transition_function_it->second;
    } else {
        throw InvalidFSMTransition(transition, state_.load());
    }
}

void FSM::set_state(FSM::State new_state) {
    state_.store(new_state);
    last_changed_.store(std::chrono::system_clock::now());
    LOG(logger_, STATUS) << "New state: " << new_state;

    // Pass state to callbacks
    call_state_callbacks();
}

void FSM::set_status(std::string status) {
    const std::lock_guard status_lock {status_mutex_};

    // Store the status message and reset emission flag if new:
    if(status != status_) {
        LOG(logger_, DEBUG) << "Setting new status: " << status;
        status_ = std::move(status);
        status_emitted_.store(false);
    }
}

std::string_view FSM::getStatus() const {
    const std::lock_guard status_lock {status_mutex_};
    return status_;
}

bool FSM::isAllowed(Transition transition) const {
    try {
        find_transition_function(transition);
    } catch(const FSMError&) {
        return false;
    }
    return true;
}

void FSM::react(Transition transition, TransitionPayload payload) {
    // Acquire lock to prevent other threads from setting state
    const std::lock_guard transition_lock {transition_mutex_};
    // Find transition
    auto transition_function = find_transition_function(transition);

    LOG(logger_, INFO) << "Reacting to transition " << transition;
    // Execute transition function
    const auto new_state = (this->*transition_function)(std::move(payload));
    set_state(new_state);
}

bool FSM::reactIfAllowed(Transition transition, TransitionPayload payload) {
    try {
        react(transition, std::move(payload));
    } catch(const FSMError&) {
        LOG(logger_, DEBUG) << "Skipping transition " << transition;
        return false;
    }
    return true;
}

std::pair<CSCP1Message::Type, std::string> FSM::reactCommand(TransitionCommand transition_command,
                                                             const PayloadBuffer& payload) {
    // Cast to normal transition, underlying values are identical
    auto transition = static_cast<Transition>(transition_command);
    LOG(logger_, INFO) << "Reacting to transition " << transition;
    // Acquire lock to prevent other threads from setting state
    const std::lock_guard transition_lock {transition_mutex_};
    // Check if command is a valid transition for the current state
    TransitionFunction transition_function {};
    try {
        transition_function = find_transition_function(transition);
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
    if(should_have_payload && payload.empty()) {
        std::string payload_info {"Transition " + to_string(transition) + " requires a payload frame"};
        LOG(logger_, WARNING) << payload_info;
        return {CSCP1Message::Type::INCOMPLETE, std::move(payload_info)};
    }
    // If there is a payload, but it is not used add a note in the reply
    const std::string payload_note = (!should_have_payload && !payload.empty()) ? " (payload frame is ignored)"s : ""s;

    // Try to decode the payload:
    TransitionPayload fsm_payload {};
    try {
        if(!payload.empty()) {
            if(transition == Transition::initialize || transition == Transition::reconfigure) {
                fsm_payload = Configuration(Dictionary::disassemble(payload));
            } else if(transition == Transition::start) {
                fsm_payload = msgpack_unpack_to<std::string>(to_char_ptr(payload.span().data()), payload.span().size());
                if(!CSCP::is_valid_run_id(std::get<std::string>(fsm_payload))) {
                    throw InvalidPayload("Run identifier contains invalid characters");
                }
            }
        }
    } catch(const InvalidPayload& error) {
        std::string payload_info {"Transition " + to_string(transition) + " received invalid payload: " + error.what()};
        LOG(logger_, WARNING) << payload_info;
        return {CSCP1Message::Type::INCOMPLETE, std::move(payload_info)};
    } catch(const MsgpackUnpackError&) {
        std::string payload_info {"Transition " + to_string(transition) + " received incorrect payload"};
        LOG(logger_, WARNING) << payload_info;
        return {CSCP1Message::Type::INCOMPLETE, std::move(payload_info)};
    }

    // Execute transition function
    const auto new_state = (this->*transition_function)(std::move(fsm_payload));
    set_state(new_state);

    // Return that command is being executed
    return {CSCP1Message::Type::SUCCESS, "Transition " + to_string(transition) + " is being initiated" + payload_note};
}

void FSM::requestInterrupt(std::string_view reason) {
    LOG(logger_, DEBUG) << "Attempting to interrupt...";

    // Wait until we are in a steady state
    while(!is_steady(state_.load())) {
        LOG_ONCE(logger_, DEBUG) << "Waiting for a steady state...";
    }

    const auto msg = "Interrupting satellite operation: " + std::string(reason);

    // In a steady state, try to react to interrupt and pass the reason as payload:
    const auto interrupting = reactIfAllowed(Transition::interrupt, {msg});

    if(interrupting) {
        LOG(logger_, WARNING) << msg;

        // We could be in interrupting, so wait for steady state
        while(!is_steady(state_.load())) {
            LOG_ONCE(logger_, DEBUG) << "Waiting for a steady state...";
        }
    } else {
        LOG(logger_, DEBUG) << "Interrupt in current state not allowed";
    }
}

void FSM::requestFailure(std::string_view reason) {
    LOG(logger_, DEBUG) << "Attempting to trigger failure...";

    // Wait until we are in a steady state
    while(!is_steady(state_.load())) {
        LOG_ONCE(logger_, DEBUG) << "Waiting for a steady state...";
    }

    // Trigger failure
    const auto failing = reactIfAllowed(Transition::failure);
    LOG(logger_, failing ? CRITICAL : WARNING)
        << "Failure during satellite operation: " << reason << (failing ? "" : " (skipped transition, already in ERROR)");
}

void FSM::registerStateCallback(const std::string& identifier, std::function<void(State, std::string_view)> callback) {
    const std::lock_guard state_callbacks_lock {state_callbacks_mutex_};
    state_callbacks_.emplace(identifier, std::move(callback));
}

void FSM::unregisterStateCallback(const std::string& identifier) {
    const std::lock_guard state_callbacks_lock {state_callbacks_mutex_};
    state_callbacks_.erase(identifier);
}

void FSM::terminate() {
    stop_run_thread();
    join_transitional_thread();
    join_failure_thread();
}

void FSM::call_state_callbacks() {
    const std::lock_guard state_callbacks_lock {state_callbacks_mutex_};

    // Fetch the status message unless emitted already
    std::unique_lock status_lock {status_mutex_};
    const auto status = (status_emitted_.load() ? "" : status_);
    status_emitted_.store(true);
    status_lock.unlock();

    std::vector<std::future<void>> futures {};
    futures.reserve(state_callbacks_.size());
    for(const auto& [id, callback] : state_callbacks_) {
        futures.emplace_back(std::async(std::launch::async, [&]() {
            try {
                callback(state_.load(), status);
            } catch(...) {
                LOG(logger_, WARNING) << "State callback " << std::quoted(id) << " threw an exception";
            }
        }));
    }
    for(const auto& future : futures) {
        future.wait();
    }
}

void FSM::stop_run_thread() {
    LOG(logger_, TRACE) << "Stopping running function of satellite...";
    run_thread_.request_stop();
    if(run_thread_.joinable()) {
        LOG(logger_, DEBUG) << "Joining running function of satellite...";
        run_thread_.join();
    }
}

void FSM::join_transitional_thread() {
    if(transitional_thread_.joinable()) {
        LOG(logger_, DEBUG) << "Joining transitional function of satellite...";
        transitional_thread_.join();
    }
}

void FSM::join_failure_thread() {
    if(failure_thread_.joinable()) {
        LOG(logger_, DEBUG) << "Joining failure function of satellite...";
        failure_thread_.join();
    }
}

// Calls the transition function of a satellite and return success transition if completed or failure on exception
template <typename Func, typename... Args>
FSM::Transition FSM::call_satellite_function(Func func, Transition success_transition, Args&&... args) {
    std::string error_message {};
    try {
        // Call transition function of satellite
        const auto status = (satellite_->*func)(std::forward<Args>(args)...);

        if(status.has_value()) {
            set_status(status.value());
        }

        // Finish transition
        return success_transition;
    } catch(const std::exception& error) {
        error_message = error.what();
    } catch(...) {
        error_message = "<unknown exception>";
    }
    // Something went wrong, log and go to error state
    LOG(satellite_->logger_, CRITICAL) << "Critical failure during transition: " << error_message;
    set_status("Critical failure during transition: " + error_message);
    return Transition::failure;
}

namespace {
    // Joins a thread and assigns it to a new thread with given args
    template <typename T, typename... Args> void launch_assign_thread(T& thread, Args&&... args) {
        // Join if possible to avoid std::terminate
        if(thread.joinable()) {
            thread.join();
        }
        // Launch thread
        thread = T(std::forward<Args>(args)...);
    }
} // namespace

// NOLINTBEGIN(performance-unnecessary-value-param,readability-convert-member-functions-to-static)

FSM::State FSM::initialize(TransitionPayload payload) {
    auto call_wrapper = [this](Configuration&& config) {
        // First join failure thread
        join_failure_thread();

        LOG(logger_, INFO) << "Calling initializing function of satellite...";
        const auto transition =
            call_satellite_function(&BaseSatellite::initializing_wrapper, Transition::initialized, std::move(config));
        react(transition);
    };
    launch_assign_thread(transitional_thread_, call_wrapper, std::get<Configuration>(std::move(payload)));
    return State::initializing;
}

FSM::State FSM::initialized(TransitionPayload /* payload */) {
    return State::INIT;
}

FSM::State FSM::launch(TransitionPayload /* payload */) {
    auto call_wrapper = [this]() {
        LOG(logger_, INFO) << "Calling launching function of satellite...";
        const auto transition = call_satellite_function(&BaseSatellite::launching_wrapper, Transition::launched);
        react(transition);
    };
    launch_assign_thread(transitional_thread_, call_wrapper);
    return State::launching;
}

FSM::State FSM::launched(TransitionPayload /* payload */) {
    return State::ORBIT;
}

FSM::State FSM::land(TransitionPayload /* payload */) {
    auto call_wrapper = [this]() {
        LOG(logger_, INFO) << "Calling landing function of satellite...";
        const auto transition = call_satellite_function(&BaseSatellite::landing_wrapper, Transition::landed);
        react(transition);
    };
    launch_assign_thread(transitional_thread_, call_wrapper);
    return State::landing;
}

FSM::State FSM::landed(TransitionPayload /* payload */) {
    return State::INIT;
}

FSM::State FSM::reconfigure(TransitionPayload payload) {
    auto call_wrapper = [this](Configuration&& partial_config) {
        LOG(logger_, INFO) << "Calling reconfiguring function of satellite...";
        const auto transition = call_satellite_function(
            &BaseSatellite::reconfiguring_wrapper, Transition::reconfigured, std::move(partial_config));
        react(transition);
    };
    launch_assign_thread(transitional_thread_, call_wrapper, std::get<Configuration>(std::move(payload)));
    return State::reconfiguring;
}

FSM::State FSM::reconfigured(TransitionPayload /* payload */) {
    return State::ORBIT;
}

FSM::State FSM::start(TransitionPayload payload) {
    auto call_wrapper = [this](std::string&& run_id) {
        LOG(logger_, INFO) << "Calling starting function of satellite...";
        const auto transition =
            call_satellite_function(&BaseSatellite::starting_wrapper, Transition::started, std::move(run_id));
        react(transition);
    };
    launch_assign_thread(transitional_thread_, call_wrapper, std::get<std::string>(std::move(payload)));
    return State::starting;
}

FSM::State FSM::started(TransitionPayload /* payload */) {
    // Start running thread async
    auto call_wrapper = std::bind_front(&BaseSatellite::running_wrapper, satellite_);
    launch_assign_thread(run_thread_, call_wrapper);
    return State::RUN;
}

FSM::State FSM::stop(TransitionPayload /* payload */) {
    auto call_wrapper = [this]() {
        // First stop RUN thread
        stop_run_thread();

        LOG(logger_, INFO) << "Calling stopping function of satellite...";
        const auto transition = call_satellite_function(&BaseSatellite::stopping_wrapper, Transition::stopped);
        react(transition);
    };
    launch_assign_thread(transitional_thread_, call_wrapper);
    return State::stopping;
}

FSM::State FSM::stopped(TransitionPayload /* payload */) {
    return State::ORBIT;
}

FSM::State FSM::interrupt(TransitionPayload payload) {
    // Set status message with information from payload:
    if(std::holds_alternative<std::string>(payload)) {
        set_status(std::get<std::string>(std::move(payload)));
    }

    auto call_wrapper = [this](State previous_state) {
        // First stop RUN thread if in RUN
        if(previous_state == State::RUN) {
            stop_run_thread();
        }

        LOG(logger_, INFO) << "Calling interrupting function of satellite...";
        const auto transition =
            call_satellite_function(&BaseSatellite::interrupting_wrapper, Transition::interrupted, previous_state);
        react(transition);
    };
    launch_assign_thread(transitional_thread_, call_wrapper, state_.load());
    return State::interrupting;
}

FSM::State FSM::interrupted(TransitionPayload /* payload */) {
    return State::SAFE;
}

FSM::State FSM::failure(TransitionPayload /* payload */) {
    auto call_wrapper = [this](State previous_state) {
        // First stop RUN thread if in RUN
        if(previous_state == State::RUN) {
            stop_run_thread();
        }

        LOG(logger_, INFO) << "Calling failure function of satellite...";
        call_satellite_function(&BaseSatellite::failure_wrapper, Transition::failure, previous_state);
        // Note: we do not trigger a success transition as we always go to ERROR state
    };
    launch_assign_thread(failure_thread_, call_wrapper, state_.load());
    return State::ERROR;
}

// NOLINTEND(performance-unnecessary-value-param,readability-convert-member-functions-to-static)
