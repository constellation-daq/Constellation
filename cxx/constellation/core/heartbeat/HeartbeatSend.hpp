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

    class HeartbeatSend final {
    public:
        CNSTLN_API HeartbeatSend(std::string_view sender, std::chrono::milliseconds interval);

        CNSTLN_API ~HeartbeatSend();

        // No copy/move constructor/assignment
        HeartbeatSend(const HeartbeatSend& other) = delete;
        HeartbeatSend& operator=(const HeartbeatSend& other) = delete;
        HeartbeatSend(HeartbeatSend&& other) = delete;
        HeartbeatSend& operator=(HeartbeatSend&& other) = delete;

        /**
         * Get ephemeral port to which the CHP socket is bound
         *
         * @return Port number
         */
        constexpr utils::Port getPort() const { return port_; }

        void updateInterval(std::chrono::milliseconds interval) { interval_ = interval; }

        void updateState(message::State state) {
            state_ = state;
            cv_.notify_one();
        }

    private:
        void loop(const std::stop_token& stop_token);

        zmq::context_t context_;
        zmq::socket_t pub_;
        utils::Port port_;

        std::string sender_;
        message::State state_ {message::State::NEW};
        std::chrono::milliseconds interval_;

        std::condition_variable cv_;
        std::mutex mutex_;
        std::jthread sender_thread_;
    };

} // namespace constellation::heartbeat
