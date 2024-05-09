/**
 * @file
 * @brief Heartbeat sender
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <condition_variable>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/ports.hpp"

namespace constellation::heartbeat {

    /** Sender class which emits heartbeat messages in regular intervals as well as extrasystoles at state changes */
    class HeartbeatSend final {
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
        CNSTLN_API HeartbeatSend(std::string_view sender, std::chrono::milliseconds interval);

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
         * @brief Update the heartbeat interval to a new value
         *
         * @param interval New heartbeat interval
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
        message::State state_ {message::State::NEW};
        /** Heartbeat broadcasting interval */
        std::chrono::milliseconds interval_;

        std::condition_variable cv_;
        std::mutex mutex_;
        std::jthread sender_thread_;
    };

} // namespace constellation::heartbeat
