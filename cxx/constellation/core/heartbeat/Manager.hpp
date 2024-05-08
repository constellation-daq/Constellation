/**
 * @file
 * @brief Heartbeat Manager
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <string_view>
#include <thread>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/heartbeat/HeartbeatRecv.hpp"
#include "constellation/core/heartbeat/HeartbeatSend.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/ports.hpp"

namespace constellation::heartbeat {

    /**
     * Manager for CHP publishing and receiving
     *
     * This manager holds a heartbeat sender and receiver as well as the logic for calling FSM interrupts based on
     * received heartbeats. It keeps track of received heartbeats from remote heartbeat senders, counts their lives and
     * takes action either upon missing heartbeats or a remote ERROR state of the FSM.
     */
    class HeartbeatManager {
    public:
        /**
         * Construct a heartbeat manager
         *
         * The constructor directly starts sender and receiver as well as the manager's watchdog thread which keeps track
         * of remote heartbeat rates and states
         *
         * @param sender Canonical name of the heartbeat sender
         */
        CNSTLN_API HeartbeatManager(std::string_view sender);

        /** Deconstruct the manager. This stops the watchdog thread */
        CNSTLN_API virtual ~HeartbeatManager();

        // No copy/move constructor/assignment
        HeartbeatManager(const HeartbeatManager& other) = delete;
        HeartbeatManager& operator=(const HeartbeatManager& other) = delete;
        HeartbeatManager(HeartbeatManager&& other) = delete;
        HeartbeatManager& operator=(HeartbeatManager&& other) = delete;

        /**
         * @brief Update the current state to be broadcasted
         * @details This method will update the state of the FSM to be broadcasted via CHP. Updating the state will also
         * trigger the emission of an extrasystole CHP message with the new state
         *
         * @param state New state
         */
        CNSTLN_API void updateState(message::State state);

        /**
         * @brief Obtain the current state registered from a given remote
         * @details [long description]
         *
         * @param remote Canonical name of the remote in question
         * @return Currently registered state of the remote if remote is present, empty optional otherwise
         */
        CNSTLN_API std::optional<message::State> getRemoteState(std::string_view remote);

        /**
         * @brief Set the interrupt callback
         * @details This function allows setting the interrupt callback which is invoked when a remote heartbeat sender
         * reports an ERROR state or stopped sending heartbeats
         *
         * @param fct [description]
         */
        CNSTLN_API void setInterruptCallback(std::function<void()> fct) { interrupt_callback_ = std::move(fct); }

    private:
        /**
         * @brief Helper to process heartbeats. This is registered as callback in the heartbeat receiver
         * @details It registers and updates the last heartbeat time point as well as the received state from remote
         * heartbeat services
         *
         * @param msg Received CHP message from remote service
         * */
        void process_heartbeat(const message::CHP1Message& msg);

        /**
         * @brief Main loop of the manager which checks for heartbeats of registered remotes.
         * @details The thread sleeps until the next remote is expected to have sent a heartbeat, checks if any of the
         * heartbeats are late or missing and goes back to sleep. This thread holds the main logic for autonomous operation,
         * the reaction to remote ERROR states and the counting of lives as specified by the CHP protocol.
         *
         * @param stop_token Stop token to interrupt the thread
         */
        void run(const std::stop_token& stop_token);

        /** Receiver service */
        HeartbeatRecv receiver_;
        /** Sender service */
        HeartbeatSend sender_;

        /** Interrupt callback invoked upon remote failure condition and missing heartbeats */
        std::function<void()> interrupt_callback_;

        /**
         * @struct Remote
         * @brief Struct holding all relevant information for a remote CHP host
         */
        struct Remote {
            // TODO(simonspa) add importance here
            std::chrono::milliseconds interval;
            std::chrono::system_clock::time_point last_heartbeat;
            message::State last_state;
            std::size_t lives {3};
        };

        /** Map of remotes this manager tracks */
        std::map<std::string, Remote, std::less<>> remotes_;
        std::mutex mutex_;
        std::condition_variable cv_;

        log::Logger logger_;

        std::jthread watchdog_thread_;
    };
} // namespace constellation::heartbeat
