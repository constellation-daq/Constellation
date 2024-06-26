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
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/ports.hpp"

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
         * @param interval Interval at which the heartbeats are sent
         */
        CNSTLN_API HeartbeatSend(std::string sender, std::chrono::milliseconds interval);

        /** Destructor which unregisters the CHIRP heartbeat service and stops the heartbeat thread */
        CNSTLN_API ~HeartbeatSend();

        // No copy/move constructor/assignment
        HeartbeatSend(const HeartbeatSend& other) = delete;
        HeartbeatSend& operator=(const HeartbeatSend& other) = delete;
        HeartbeatSend(HeartbeatSend&& other) = delete;
        HeartbeatSend& operator=(HeartbeatSend&& other) = delete;

        /**
         * @brief Get ephemeral port to which the CHP socket is bound
         *
         * @return Port number
         */
        constexpr utils::Port getPort() const { return port_; }

        /**
         * @brief Update the maximum heartbeat interval to a new value
         *
         * @note Heartbeats are send roughly twice as often as the maximum heartbeat interval
         *
         * @param interval New maximum heartbeat interval
         */
        void updateInterval(std::chrono::milliseconds interval) { interval_ = interval; }

        /**
         * @brief Update the currently emitted state
         *
         * @param state State to be broadcasted
         */
        CNSTLN_API void updateState(message::State state);

    private:
        /**
         * Main loop sending the heartbeats.
         *
         * Between heartbeats the thread sleeps and is only woken up for the emission of
         * extrasystoles at state changes or the next regular heartbeat message after the configured interval
         */
        void loop(const std::stop_token& stop_token);

        /** ZMQ context for the emitting socket */
        zmq::context_t context_;
        /** Publisher socket for emitting heartbeats */
        zmq::socket_t pub_;
        /** Ephemeral port selected for the heartbeat emission */
        utils::Port port_;

        /** Canonical sender name */
        std::string sender_;
        /** Currently broadcasted state */
        std::atomic<message::State> state_ {message::State::NEW};
        /** Maximum heartbeat broadcasting interval */
        std::atomic<std::chrono::milliseconds> interval_;

        std::condition_variable cv_;
        std::mutex mutex_;
        std::jthread sender_thread_;
    };

} // namespace constellation::heartbeat
