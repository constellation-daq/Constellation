/**
 * @file
 * @brief FSM class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <chrono>
#include <compare>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

namespace constellation::satellite {
    class BaseSatellite;

    class FSM {
    public:
        using State = protocol::CSCP::State;
        using Transition = protocol::CSCP::Transition;
        using TransitionCommand = protocol::CSCP::TransitionCommand;

        /** Payload of a transition function: variant with configuration or run identifier */
        using TransitionPayload = std::variant<std::monostate, config::Configuration, std::string>;

        /** Function pointer for a transition function: takes the variant mentioned above, returns new State */
        using TransitionFunction = State (FSM::*)(TransitionPayload);

        /** Maps the allowed transitions of a state to a transition function */
        using TransitionMap = std::map<Transition, TransitionFunction>;

        /** Maps state to transition maps for that state */
        using StateTransitionMap = std::map<State, TransitionMap>;

    private:
        class Condition {
        public:
            /**
             * @brief Construct a new condition
             *
             * @param remote Canonical name of the remote corresponding to the condition
             * @param state Station for which the condition applies
             */
            Condition(std::string remote, State state) : remote_(std::move(remote)), state_(state) {}

            /**
             * @brief Get the remote corresponding to the transition
             *
             * @return Name of the remote
             */
            std::string_view getRemote() const { return remote_; }

            /**
             * @brief Check if the condition applies to the current state
             *
             * @return True if the condition applies, false otherwise
             */
            bool applies(State state) const { return (state_ == state); }

            /**
             * @brief Check if the condition is satisfied
             *
             * @param state State of the remote
             * @return True if the condition is satisfied, false otherwise
             */
            bool isSatisfied(State state) const { return protocol::CSCP::transitions_to(state_, state); }

            std::strong_ordering operator<=>(const Condition& other) const;

        private:
            std::string remote_;
            State state_;
        };

    public:
        /**
         * @brief Construct the final state machine of a satellite
         *
         * @param satellite Satellite class with functions of transitional states
         */
        FSM(BaseSatellite* satellite)
            : last_changed_(std::chrono::system_clock::now()), satellite_(satellite), logger_("FSM") {}

        CNSTLN_API ~FSM();

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        FSM(const FSM& other) = delete;
        FSM& operator=(const FSM& other) = delete;
        FSM(FSM&& other) = delete;
        FSM& operator=(FSM&& other) = delete;
        /// @endcond

        /**
         * @brief Returns the current state of the FSM
         */
        State getState() const { return state_.load(); }

        /**
         * @brief Returns the current status message of the FSM
         */
        CNSTLN_API std::string_view getStatus() const;

        /**
         * @brief Return the timestamp of the last state change
         */
        std::chrono::system_clock::time_point getLastChanged() const { return last_changed_.load(); }

        /**
         * @brief Check if a FSM transition is allowed in current state
         *
         * @param transition Transition to check if allowed
         * @return True if transition is possible
         */
        CNSTLN_API bool isAllowed(Transition transition) const;

        /**
         * @brief Perform a FSM transition
         *
         * @param transition Transition to perform
         * @param payload Payload for the transition function
         * @throw FSMError if the transition is not a valid transition in the current state
         */
        CNSTLN_API void react(Transition transition, TransitionPayload payload = {});

        /**
         * @brief Perform a FSM transition if allowed, otherwise do nothing
         *
         * @param transition Transition to perform if allowed
         * @param payload Payload for the transition function
         * @return True if the transition was initiated
         */
        CNSTLN_API bool reactIfAllowed(Transition transition, TransitionPayload payload = {});

        /**
         * @brief Perform a FSM transition via a CSCP message
         *
         * @param transition_command Transition command from CSCP
         * @param payload Payload frame from CSCP
         * @return Tuple containing the CSCP message type and a description
         */
        CNSTLN_API std::pair<message::CSCP1Message::Type, std::string> reactCommand(TransitionCommand transition_command,
                                                                                    const message::PayloadBuffer& payload);

        /**
         * @brief Try to perform an interrupt as soon as possible
         *
         * This function waits for the next steady state and performs an interrupt if in ORBIT or RUN state, otherwise
         * nothing is done. It guarantees that the FSM is in a state where the satellite can be safely shut down.
         *
         * @param reason Reason for the requested interrupt
         *
         * @warning This function is not thread safe, meaning that no other react command should be called during execution.
         */
        CNSTLN_API void requestInterrupt(std::string_view reason);

        /**
         * @brief Try to perform a failure as soon as possible
         *
         * This function waits for the next steady state and performs a failure if not in ERROR, otherwise nothing is done.
         *
         * @param reason Reason for the requested failure
         *
         * @warning This function is not thread safe, meaning that no other react command should be called during execution.
         */
        CNSTLN_API void requestFailure(std::string_view reason);

        /**
         * @brief Registering a callback to be executed when a new state was entered
         *
         * This function adds a new state update callback. Registered callbacks are used to distribute the state of the FSM
         * whenever it was changed.
         *
         * @warning State callbacks block the execution of further transitions, callbacks that take a long time should
         *          offload the work to a new thread.
         *
         * @param identifier Identifier string for this callback
         * @param callback Callback taking the new state as argument
         */
        CNSTLN_API void registerStateCallback(const std::string& identifier,
                                              std::function<void(State, std::string_view)> callback);

        /**
         * @brief Unregistering a state callback
         *
         * This function removed the state update callback with the given identifier
         *
         * @param identifier Identifier string for this callback
         */
        CNSTLN_API void unregisterStateCallback(const std::string& identifier);

        /**
         * @brief Registering a callback which allows to fetch the state of a remote satellite
         *
         * This function registers a remote state callback which allows the FSM to query the last known state of a remote
         * satellite, e.g. for conditional transitions
         *
         * @param callback Callback taking the name of the remote satellite as argument and returning an optional with its
         * state, if known, or a `std::nullopt` if not known
         */
        CNSTLN_API void registerRemoteCallback(std::function<std::optional<State>(std::string_view)> callback);

        /**
         * @brief Terminate all FSM threads
         */
        CNSTLN_API void terminate();

    private:
        /**
         * @brief Find the transition function for a given transition in the current state
         *
         * @param transition Transition to search for a transition function
         * @return Transition function corresponding to the transition
         * @throw FSMError if the transition is not a valid transition in the current state
         */
        TransitionFunction find_transition_function(Transition transition) const;

        /**
         * @brief Set a new state and update the last changed timepoint
         */
        void set_state(State new_state);

        /**
         * @brief Set a new status message
         */
        void set_status(std::string status);

        /**
         * @brief Call all state callbacks
         *
         * @param only_with_status Boolean to set whether callbacks are always called or only with a new status
         */
        void call_state_callbacks(bool only_with_status = false);

        /**
         * @brief Call a satellite function
         *
         * Calls the wrapper function of the BaseSatellite and sets the status depending on the return value.
         *
         * @param func Function to be called
         * @param args Function arguments
         * @return True if the function returned without any issue, false if there was an exception
         */
        template <typename Func, typename... Args> bool call_satellite_function(Func func, Args&&... args);

        /**
         * @brief Stop and join the run_thread
         */
        void stop_run_thread();

        /**
         * @brief Join the transitional_thread_
         */
        void join_transitional_thread();

        /**
         * @brief Join the failure_thread
         */
        void join_failure_thread();

        /**
         * @brief Read FSM-relevant parameters from the initialization configuration
         *
         * This function registers transition conditions for remote satellites. The FSM will query remote states from
         * its remote callback to satisfy these conditions before locally executing the requested transition.
         *
         * @param config Satellite configuration
         */
        void initialize_fsm(config::Configuration& config);

        CNSTLN_API auto initialize(TransitionPayload payload) -> State;
        CNSTLN_API auto initialized(TransitionPayload payload) -> State;
        CNSTLN_API auto launch(TransitionPayload payload) -> State;
        CNSTLN_API auto launched(TransitionPayload payload) -> State;
        CNSTLN_API auto land(TransitionPayload payload) -> State;
        CNSTLN_API auto landed(TransitionPayload payload) -> State;
        CNSTLN_API auto reconfigure(TransitionPayload payload) -> State;
        CNSTLN_API auto reconfigured(TransitionPayload payload) -> State;
        CNSTLN_API auto start(TransitionPayload payload) -> State;
        CNSTLN_API auto started(TransitionPayload payload) -> State;
        CNSTLN_API auto stop(TransitionPayload payload) -> State;
        CNSTLN_API auto stopped(TransitionPayload payload) -> State;
        CNSTLN_API auto interrupt(TransitionPayload payload) -> State;
        CNSTLN_API auto interrupted(TransitionPayload payload) -> State;
        CNSTLN_API auto failure(TransitionPayload payload) -> State;

        // clang-format off
        const StateTransitionMap state_transition_map_ { // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
            {State::NEW, {
                {Transition::initialize, &FSM::initialize},
                {Transition::failure, &FSM::failure},
            }},
            {State::initializing, {
                {Transition::initialized, &FSM::initialized},
                {Transition::failure, &FSM::failure},
            }},
            {State::INIT, {
                {Transition::initialize, &FSM::initialize},
                {Transition::launch, &FSM::launch},
                {Transition::failure, &FSM::failure},
            }},
            {State::launching, {
                {Transition::launched, &FSM::launched},
                {Transition::failure, &FSM::failure},
            }},
            {State::landing, {
                {Transition::landed, &FSM::landed},
                {Transition::failure, &FSM::failure},
            }},
            {State::ORBIT, {
                {Transition::land, &FSM::land},
                {Transition::reconfigure, &FSM::reconfigure},
                {Transition::start, &FSM::start},
                {Transition::interrupt, &FSM::interrupt},
                {Transition::failure, &FSM::failure},
            }},
            {State::reconfiguring, {
                {Transition::reconfigured, &FSM::reconfigured},
                {Transition::failure, &FSM::failure},
            }},
            {State::starting, {
                {Transition::started, &FSM::started},
                {Transition::failure, &FSM::failure},
            }},
            {State::stopping, {
                {Transition::stopped, &FSM::stopped},
                {Transition::failure, &FSM::failure},
            }},
            {State::RUN, {
                {Transition::stop, &FSM::stop},
                {Transition::interrupt, &FSM::interrupt},
                {Transition::failure, &FSM::failure},
            }},
            {State::interrupting, {
                {Transition::interrupted, &FSM::interrupted},
                {Transition::failure, &FSM::failure},
            }},
            {State::SAFE, {
                {Transition::initialize, &FSM::initialize},
                {Transition::failure, &FSM::failure},
            }},
            {State::ERROR, {
                {Transition::initialize, &FSM::initialize},
            }},
        };
        // clang-format on

    private:
        std::atomic<State> state_ {State::NEW};
        std::atomic<std::chrono::system_clock::time_point> last_changed_;

        mutable std::mutex status_mutex_;
        std::string status_;
        std::atomic<bool> status_emitted_;

        BaseSatellite* satellite_;
        log::Logger logger_;
        std::mutex transition_mutex_;
        std::thread transitional_thread_;
        std::jthread run_thread_;
        std::thread failure_thread_;

        /** State update callback */
        std::map<std::string, std::function<void(State, std::string_view)>> state_callbacks_;
        std::mutex state_callbacks_mutex_;

        /** Remote state callback */
        std::function<std::optional<State>(std::string_view)> remote_callback_;
        std::set<Condition> remote_conditions_;
        std::chrono::seconds remote_condition_timeout_ {60};
    };

} // namespace constellation::satellite
