/**
 * @file
 * @brief FSM class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "FSM.hpp"

#include <algorithm>
#include <chrono>
#include <compare>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <msgpack.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/exceptions.hpp"
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
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/BaseSatellite.hpp"
#include "constellation/satellite/exceptions.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;
using namespace std::string_literals;

std::strong_ordering FSM::Condition::operator<=>(const Condition& other) const {
    const auto ord_remote = remote_ <=> other.remote_;
    if(std::is_eq(ord_remote)) {
        return state_ <=> other.state_;
    }
    return ord_remote;
}

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

    const auto msg = "Failure during satellite operation: " + std::string(reason);

    // Trigger failure and pass reason as payload
    const auto failing = reactIfAllowed(Transition::failure, {msg});
    LOG(logger_, failing ? CRITICAL : WARNING) << msg << (failing ? "" : " (skipped transition, already in ERROR)");
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

void FSM::registerRemoteCallback(std::function<std::optional<State>(std::string_view)> callback) {
    remote_callback_ = std::move(callback);
}

void FSM::call_state_callbacks(bool only_with_status) {
    const std::lock_guard state_callbacks_lock {state_callbacks_mutex_};

    // Check if the status has been emitted:
    if(status_emitted_.load() && only_with_status) {
        return;
    }

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
                LOG(logger_, WARNING) << "State callback " << quote(id) << " threw an exception";
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

// Calls the wrapper function of the BaseSatellite and returns if completed or failure on exception
template <typename Func, typename... Args> bool FSM::call_satellite_function(Func func, Args&&... args) {
    std::string error_message {};

    // Check if transition conditions are satisfied:
    if(remote_callback_ && !remote_conditions_.empty()) {
        LOG(logger_, INFO) << "Checking remote conditions...";

        // Start timer for remote conditions
        TimeoutTimer timer {remote_condition_timeout_};
        timer.reset();

        while(true) {
            bool satisfied = true;
            for(const auto& condition : remote_conditions_) {
                // Check if this condition applies to current state:
                if(condition.applies(state_.load())) {
                    // Get remote state:
                    const auto remote_state = remote_callback_(condition.getRemote());

                    // Fail if the satellite to which this condition applies is not present in the constellation
                    if(!remote_state.has_value()) {
                        error_message = "Dependent remote satellite " + quote(condition.getRemote()) + " not present";
                        LOG(logger_, CRITICAL) << "Critical failure: " << error_message;
                        set_status("Critical failure: " + error_message);
                        return false;
                    }

                    // Check if state is ERROR:
                    if(remote_state.value() == State::ERROR) {
                        error_message = "Dependent remote satellite " + quote(condition.getRemote()) + " reports state " +
                                        quote(enum_name(remote_state.value()));
                        LOG(logger_, CRITICAL) << "Critical failure: " << error_message;
                        set_status("Critical failure: " + error_message);
                        return false;
                    }

                    // Check if condition is fulfilled:
                    if(!condition.isSatisfied(remote_state.value())) {
                        auto msg = "Awaiting state from " + quote(condition.getRemote()) + ", currently reporting state " +
                                   quote(enum_name(remote_state.value()));
                        LOG_T(logger_, DEBUG, 1s) << msg;

                        // Set status message and emit if new:
                        set_status(std::move(msg));
                        call_state_callbacks(true);

                        satisfied = false;
                        break;
                    }
                }
            }

            // If all conditions are satisfied, continue:
            if(satisfied) {
                LOG(logger_, INFO) << "Satisfied with all remote conditions, continuing";
                break;
            }

            // If timeout reached, throw
            if(timer.timeoutReached()) {
                error_message =
                    "Could not satisfy remote conditions within " + to_string(remote_condition_timeout_) + " timeout";
                LOG(logger_, CRITICAL) << "Critical failure: " << error_message;
                set_status("Critical failure: " + error_message);
                return false;
            }

            // Wait a bit before checking again
            std::this_thread::sleep_for(10ms);
        }
    }

    try {
        // Call function of satellite
        const auto status = (satellite_->*func)(std::forward<Args>(args)...);

        // Set status if returned
        if(status.has_value()) {
            set_status(status.value());
        }

        return true;
    } catch(const std::exception& error) {
        error_message = error.what();
    } catch(...) {
        error_message = "<unknown exception>";
    }
    // Something went wrong, log and return false
    std::string failure_message = "Critical failure: " + error_message;
    LOG(satellite_->logger_, CRITICAL) << failure_message;
    set_status(std::move(failure_message));
    return false;
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

void FSM::initialize_fsm(Configuration& config) {
    // Clear previously stored conditions
    remote_conditions_.clear();

    // Parse all transition condition parameters
    for(const auto& state : {CSCP::State::initializing,
                             CSCP::State::launching,
                             CSCP::State::landing,
                             CSCP::State::starting,
                             CSCP::State::stopping}) {
        const auto key = "_require_" + to_string(state) + "_after";
        if(config.has(key)) {
            const auto remotes = config.getArray<std::string>(key);

            LOG(logger_, INFO) << "Registering condition for transitional state " << quote(to_string(state))
                               << " and remotes " << quote(range_to_string(remotes));

            std::ranges::for_each(remotes, [this, &config, &key, state](const auto& remote) {
                // Check that names are valid
                if(!CSCP::is_valid_canonical_name(remote)) {
                    throw InvalidValueError(config, key, "Not a valid canonical name");
                }

                // Check that the requested remote is not this satellite
                if(transform(remote, ::tolower) == transform(satellite_->getCanonicalName(), ::tolower)) {
                    throw InvalidValueError(config, key, "Satellite cannot depend on itself");
                }

                remote_conditions_.emplace(remote, state);
            });
        }
    }

    // Set timeout for conditional transitions
    remote_condition_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_conditional_transition_timeout", 30));
}

// NOLINTBEGIN(performance-unnecessary-value-param,readability-convert-member-functions-to-static)

FSM::State FSM::initialize(TransitionPayload payload) {
    auto call_wrapper = [this](Configuration&& config) {
        // First join failure thread
        join_failure_thread();

        // Initialize FSM itself with configuration settings
        LOG(logger_, DEBUG) << "Initializing FSM settings...";
        try {
            initialize_fsm(config);
        } catch(const std::exception& error) {
            std::string failure_message = "Critical failure: "s + error.what();
            LOG(logger_, CRITICAL) << failure_message;
            set_status(std::move(failure_message));
            react(Transition::failure);
            return;
        }

        LOG(logger_, INFO) << "Calling initializing function of satellite...";
        const auto success = call_satellite_function(&BaseSatellite::initializing_wrapper, std::move(config));
        react(success ? Transition::initialized : Transition::failure);
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
        const auto success = call_satellite_function(&BaseSatellite::launching_wrapper);
        react(success ? Transition::launched : Transition::failure);
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
        const auto success = call_satellite_function(&BaseSatellite::landing_wrapper);
        react(success ? Transition::landed : Transition::failure);
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
        const auto success = call_satellite_function(&BaseSatellite::reconfiguring_wrapper, std::move(partial_config));
        react(success ? Transition::reconfigured : Transition::failure);
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
        const auto success = call_satellite_function(&BaseSatellite::starting_wrapper, std::move(run_id));
        react(success ? Transition::started : Transition::failure);
    };
    launch_assign_thread(transitional_thread_, call_wrapper, std::get<std::string>(std::move(payload)));
    return State::starting;
}

FSM::State FSM::started(TransitionPayload /* payload */) {
    auto call_wrapper = [this](const std::stop_token& stop_token) {
        LOG(logger_, INFO) << "Calling running function of satellite...";
        const auto success = call_satellite_function(&BaseSatellite::running_wrapper, stop_token);
        if(!success) {
            react(Transition::failure);
        }
    };
    launch_assign_thread(run_thread_, call_wrapper);
    return State::RUN;
}

FSM::State FSM::stop(TransitionPayload /* payload */) {
    auto call_wrapper = [this]() {
        // First stop RUN thread
        stop_run_thread();

        LOG(logger_, INFO) << "Calling stopping function of satellite...";
        const auto success = call_satellite_function(&BaseSatellite::stopping_wrapper);
        react(success ? Transition::stopped : Transition::failure);
    };
    launch_assign_thread(transitional_thread_, call_wrapper);
    return State::stopping;
}

FSM::State FSM::stopped(TransitionPayload /* payload */) {
    return State::ORBIT;
}

FSM::State FSM::interrupt(TransitionPayload payload) {
    // Set status message with information from payload:
    std::string reason;
    if(std::holds_alternative<std::string>(payload)) {
        reason = std::get<std::string>(std::move(payload));
        set_status(reason);
    }

    auto call_wrapper = [this](State previous_state, std::string reason) {
        // First stop RUN thread if in RUN
        if(previous_state == State::RUN) {
            stop_run_thread();
        }

        LOG(logger_, INFO) << "Calling interrupting function of satellite...";
        const auto success = call_satellite_function(&BaseSatellite::interrupting_wrapper, previous_state, reason);
        react(success ? Transition::interrupted : Transition::failure);
    };
    launch_assign_thread(transitional_thread_, call_wrapper, state_.load(), std::move(reason));
    return State::interrupting;
}

FSM::State FSM::interrupted(TransitionPayload /* payload */) {
    return State::SAFE;
}

FSM::State FSM::failure(TransitionPayload payload) {
    // Set status message with information from payload:
    std::string reason;
    if(std::holds_alternative<std::string>(payload)) {
        reason = std::get<std::string>(std::move(payload));
        set_status(reason);
    }

    auto call_wrapper = [this](State previous_state, std::string reason) {
        // First stop RUN thread if in RUN
        if(previous_state == State::RUN) {
            stop_run_thread();
        }

        LOG(logger_, INFO) << "Calling failure function of satellite...";
        call_satellite_function(&BaseSatellite::failure_wrapper, previous_state, reason);
    };
    launch_assign_thread(failure_thread_, call_wrapper, state_.load(), std::move(reason));
    return State::ERROR;
}

// NOLINTEND(performance-unnecessary-value-param,readability-convert-member-functions-to-static)
