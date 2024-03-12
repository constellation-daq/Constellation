/**
 * @file
 * @brief FSM class
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <any>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include <zmq.hpp>

#include "constellation/core/config.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/satellite/fsm_definitions.hpp"

namespace constellation::satellite {
    class Satellite;

    class FSM final {
    public:
        /** Payload of a transition function: TODO(stephan.lachnit) variant with config, partial_config or run_nr */
        using TransitionPayload = std::any;

        /** Function pointer for a transition function: takes the variant mentioned above, returns new State */
        using TransitionFunction = State (FSM::*)(TransitionPayload);

        /** Maps the allowed transitions of a state to a transition function */
        using TransitionMap = std::map<Transition, TransitionFunction>;

        /** Maps state to transition maps for that state */
        using StateTransitionMap = std::map<State, TransitionMap>;

    public:
        /**
         * Construct the final state machine of a satellite
         *
         * @param satellite Satellite class with functions of transitional states
         */
        FSM(std::shared_ptr<Satellite> satellite) : satellite_(std::move(satellite)) {}

        CNSTLN_API ~FSM();

        // No copy/move constructor/assignment
        FSM(FSM& other) = delete;
        FSM& operator=(FSM other) = delete;
        FSM(FSM&& other) = delete;
        FSM& operator=(FSM&& other) = delete;

        /**
         * Returns the current state of the FSM
         *
         * @return Current state
         */
        constexpr State getState() const { return state_; }

        /**
         * Checks if a FSM transition is allowed in current state
         *
         * @param transition Transition to check if allowed
         * @return True if transition is possible
         */
        CNSTLN_API bool isAllowed(Transition transition);

        /**
         * Perform a FSM transition
         *
         * @param transition Transition to perform
         * @param payload Payload for the transition function
         * @throw FSMError if the transition is not a valid transition in the current state
         */
        CNSTLN_API void react(Transition transition, TransitionPayload payload = {});

        /**
         * Perform a FSM transition if allowed, otherwise do nothing
         *
         * TODO(stephan.lachnit): only useful for interrupt and failure, maybe private?
         *
         * @param transition Transition to perform if allowed
         * @param payload Payload for the transition function
         * @return True if the transition was initiated
         */
        CNSTLN_API bool reactIfAllowed(Transition transition, TransitionPayload payload = {});

        /**
         * Perform a FSM transition via a CSCP message
         *
         * @param transition_command Transition command from CSCP
         * @param payload Payload frame from CSCP
         * @return Tuple containing the CSCP message type and a description
         */
        CNSTLN_API std::pair<message::CSCP1Message::Type, std::string> reactCommand(TransitionCommand transition_command,
                                                                                    std::shared_ptr<zmq::message_t> payload);

    private:
        /**
         * Find the transition function for a given transition in the current state
         *
         * @param transition Transition to search for a transition function
         * @return Transition function corresponding to the transition
         * @throw FSMError if the transition is not a valid transition in the current state
         */
        TransitionFunction findTransitionFunction(Transition transition) const;

        // NOLINTBEGIN(performance-unnecessary-value-param,readability-convert-member-functions-to-static)
        CNSTLN_API auto initialize(TransitionPayload /* payload */) -> State;
        CNSTLN_API auto initialized(TransitionPayload /* payload */) -> State { return State::INIT; }
        CNSTLN_API auto launch(TransitionPayload /* payload */) -> State;
        CNSTLN_API auto launched(TransitionPayload /* payload */) -> State { return State::ORBIT; }
        CNSTLN_API auto land(TransitionPayload /* payload */) -> State;
        CNSTLN_API auto landed(TransitionPayload /* payload */) -> State { return State::INIT; }
        CNSTLN_API auto reconfigure(TransitionPayload /* payload */) -> State;
        CNSTLN_API auto reconfigured(TransitionPayload /* payload */) -> State { return State::ORBIT; }
        CNSTLN_API auto start(TransitionPayload /* payload */) -> State;
        CNSTLN_API auto started(TransitionPayload /* payload */) -> State;
        CNSTLN_API auto stop(TransitionPayload /* payload */) -> State;
        CNSTLN_API auto stopped(TransitionPayload /* payload */) -> State { return State::ORBIT; }
        CNSTLN_API auto interrupt(TransitionPayload /* payload */) -> State;
        CNSTLN_API auto interrupted(TransitionPayload /* payload */) -> State { return State::SAFE; }
        CNSTLN_API auto failure(TransitionPayload /* payload */) -> State;
        // NOLINTEND(performance-unnecessary-value-param,readability-convert-member-functions-to-static)

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
                {Transition::interrupt, &FSM::interrupt},
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
                {Transition::interrupt, &FSM::interrupt},
                {Transition::failure, &FSM::failure},
            }},
            {State::starting, {
                {Transition::started, &FSM::started},
                {Transition::interrupt, &FSM::interrupt},
                {Transition::failure, &FSM::failure},
            }},
            {State::stopping, {
                {Transition::stopped, &FSM::stopped},
                {Transition::interrupt, &FSM::interrupt},
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
        State state_ {State::NEW};
        std::shared_ptr<Satellite> satellite_;
        std::jthread transitional_thread_;
        std::jthread run_thread_;
        std::jthread failure_thread_;
    };

} // namespace constellation::satellite