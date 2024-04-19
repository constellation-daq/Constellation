/**
 * @file
 * @brief Heartbeat sender
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <zmq.hpp>

#include "constellation/core/config.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/ports.hpp"

namespace constellation::heartbeat {

    class HeartbeatSend final {
    public:
        CNSTLN_API HeartbeatSend();

        CNSTLN_API ~HeartbeatSend() = default;

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

        // start main_loop
        CNSTLN_API void sendHeartbeat(message::State state);

    private:
        zmq::context_t context_;
        zmq::socket_t pub_;
        utils::Port port_;

        log::Logger logger_;
    };

} // namespace constellation::heartbeat
