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

    private:
        HeartbeatRecv receiver_;
        HeartbeatSend sender_;

        log::Logger logger_;

        std::jthread receiver_thread_;
        std::jthread sender_thread_;
    };
} // namespace constellation::heartbeat
