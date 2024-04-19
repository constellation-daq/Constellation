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
    class Manager {
    public:
        /**
         */
        CNSTLN_API Manager(std::string_view sender);

        CNSTLN_API virtual ~Manager();

        // No copy/move constructor/assignment
        Manager(const Manager& other) = delete;
        Manager& operator=(const Manager& other) = delete;
        Manager(Manager&& other) = delete;
        Manager& operator=(Manager&& other) = delete;

        /** Start the background thread of the manager */
        CNSTLN_API void start();

        CNSTLN_API std::function<void(message::State)> getCallback();

        CNSTLN_API void setInterruptCallback(std::function<void()> fct) { interrupt_callback_ = fct; }

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
