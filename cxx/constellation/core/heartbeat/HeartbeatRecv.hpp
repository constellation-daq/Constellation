/**
 * @file
 * @brief Heartbeat receiver
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */
#pragma once

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std23.hpp"
#include "constellation/core/utils/string.hpp"

namespace constellation::heartbeat {
    class CNSTLN_API HeartbeatRecv {
    public:
        HeartbeatRecv();

        virtual ~HeartbeatRecv();

        void callback_impl(chirp::DiscoveredService service, bool depart);

        static void callback(chirp::DiscoveredService service, bool depart, std::any user_data);

        void main_loop();

    private:
        void connect(chirp::DiscoveredService service);
        void disconnect(chirp::DiscoveredService service);
        void disconnect_all();

        log::Logger logger_;
        zmq::context_t context_;
        zmq::active_poller_t poller_;
        std::map<chirp::DiscoveredService, zmq::socket_t> sockets_;
        std::mutex sockets_mutex_;
        std::condition_variable cv_;
    };
} // namespace constellation::heartbeat
