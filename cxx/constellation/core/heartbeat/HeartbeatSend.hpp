/**
 * @file
 * @brief Heartbeat sender
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/protocol/CHP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

namespace constellation::heartbeat {

    /** Sender class which emits heartbeat messages in regular intervals as well as extrasystoles at state changes */
    class HeartbeatSend {
    public:
        /**
         * @brief Construct a heartbeat sender
         *
         * This directly opens a socket, binds to an ephemeral port and starts emission of heartbeats. It also registers a
         * CHIRP heartbeat service.
         *
         * @param sender Canonical name of the sender
         * @param state_callback Function that return the current state
         * @param interval Interval at which the heartbeats are sent
         */
        CNSTLN_API HeartbeatSend(std::string sender,
                                 std::function<protocol::CSCP::State()> state_callback,
                                 std::chrono::milliseconds interval);

        /** Destructor which terminates the heartbeat sender */
        CNSTLN_API ~HeartbeatSend();

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        HeartbeatSend(const HeartbeatSend& other) = delete;
        HeartbeatSend& operator=(const HeartbeatSend& other) = delete;
        HeartbeatSend(HeartbeatSend&& other) = delete;
        HeartbeatSend& operator=(HeartbeatSend&& other) = delete;
        /// @endcond

        /**
         * @brief Terminate the heartbeat sender
         *
         * This unregisters the CHIRP heartbeat service and stops the heartbeat thread.
         */
        CNSTLN_API void terminate();

        /**
         * @brief Get ephemeral port to which the CHP socket is bound
         *
         * @return Port number
         */
        constexpr networking::Port getPort() const { return port_; }

        /**
         * @brief Set the message flags this sender is emitting
         *
         * @param flags Message flags for this sender
         */
        void setFlags(protocol::CHP::MessageFlags flags) { flags_ = flags; }

        /**
         * @brief Update the maximum heartbeat interval to a new value
         *
         * @note Heartbeats are send roughly twice as often as the maximum heartbeat interval
         *
         * @param interval New maximum heartbeat interval
         */
        void setMaximumInterval(std::chrono::milliseconds interval) { default_interval_ = interval; }

        /**
         * @brief Obtain the currently used heartbeat interval
         *
         * @return Current heartbeat interval
         */
        std::chrono::milliseconds getCurrentInterval() const { return interval_.load(); }

        /**
         * @brief Obtain the current number of subscribers
         *
         * @return Current number of heartbeat subscribers
         */
        std::size_t getSubscriberCount() const { return subscribers_.load(); }

        /**
         * @brief Send an extrasystole
         *
         * @param status Message to be attached
         */
        CNSTLN_API void sendExtrasystole(std::string status);

    private:
        /**
         * @brief Main loop sending the heartbeats
         *
         * Send heartbeats in regular intervals and checks for subscriptions
         *
         * @param stop_token Token to stop thread
         */
        void loop(const std::stop_token& stop_token);

        /**
         * @brief Send heartbeat
         *
         * @param flags CHP message flags
         * @param status Optional status message
         */
        void send_heartbeat(protocol::CHP::MessageFlags flags, std::optional<std::string> status = {});

    private:
        /** Publisher socket for emitting heartbeats */
        zmq::socket_t pub_socket_;
        /** Ephemeral port selected for the heartbeat emission */
        networking::Port port_;

        /** Canonical sender name */
        std::string sender_;
        /** Function returning the current state */
        std::function<protocol::CSCP::State()> state_callback_;

        /** Maximum heartbeat interval */
        std::atomic<std::chrono::milliseconds> default_interval_;
        /** Current number of subscribers */
        std::atomic_size_t subscribers_;
        /** Current heartbeat interval */
        std::atomic<std::chrono::milliseconds> interval_;
        /** Default message flags, defined e.g. by the role of the sender */
        std::atomic<protocol::CHP::MessageFlags> flags_;

        std::mutex mutex_;
        std::jthread sender_thread_;
    };

} // namespace constellation::heartbeat
