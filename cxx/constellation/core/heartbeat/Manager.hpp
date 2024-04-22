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

#include "constellation/core/config.hpp"
#include "constellation/core/heartbeat/HeartbeatRecv.hpp"
#include "constellation/core/heartbeat/HeartbeatSend.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/ports.hpp"

namespace constellation::heartbeat {

    /** Manager for CHP publishing and receiving */
    class HeartbeatManager {
    public:
        /**
         */
        CNSTLN_API HeartbeatManager(std::string_view sender);

        CNSTLN_API virtual ~HeartbeatManager();

        // No copy/move constructor/assignment
        HeartbeatManager(const HeartbeatManager& other) = delete;
        HeartbeatManager& operator=(const HeartbeatManager& other) = delete;
        HeartbeatManager(HeartbeatManager&& other) = delete;
        HeartbeatManager& operator=(HeartbeatManager&& other) = delete;

        /** Start the background thread of the manager */
        CNSTLN_API void start();

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

        CNSTLN_API void setInterruptCallback(std::function<void()> fct) { interrupt_callback_ = std::move(fct); }

    private:
        void process_heartbeat(const message::CHP1Message& msg);

        void run(const std::stop_token& stop_token);

        HeartbeatRecv receiver_;
        HeartbeatSend sender_;

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

        std::map<std::string, Remote, std::less<>> remotes_;
        std::mutex mutex_;
        std::condition_variable cv_;

        log::Logger logger_;

        std::jthread receiver_thread_;
        std::jthread sender_thread_;
        std::jthread watchdog_thread_;
    };
} // namespace constellation::heartbeat
