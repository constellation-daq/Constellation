/**
 * @file
 * @brief Implementation of Heartbeat sender
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "HeartbeatSend.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/logging/log.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/ports.hpp"
#include "constellation/core/utils/std23.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation;
using namespace constellation::heartbeat;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;

HeartbeatSend::HeartbeatSend(std::string_view sender, std::chrono::milliseconds interval)
    : pub_(context_, zmq::socket_type::pub), port_(bind_ephemeral_port(pub_)), sender_(sender), interval_(interval) {

    // Announce service via CHIRP
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->registerService(chirp::HEARTBEAT, port_);
    }
}

HeartbeatSend::~HeartbeatSend() {
    // Send CHIRP depart message
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->unregisterService(chirp::HEARTBEAT, port_);
    }
}

void HeartbeatSend::sendHeartbeat(State state) {
    CHP1Message(sender_, state, interval_).assemble().send(pub_);
}
