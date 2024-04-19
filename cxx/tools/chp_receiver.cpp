/**
 * @file
 * @brief CHP heartbeat sender
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <iostream>
#include <string>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/heartbeat/HeartbeatRecv.hpp"
#include "constellation/core/logging/Logger.hpp"

using namespace constellation;
using namespace constellation::heartbeat;
using namespace constellation::log;
using namespace constellation::message;
using namespace std::literals::chrono_literals;

int main(int argc, char* argv[]) {
    // Get address via cmdline
    if(argc != 2) {
        std::cout << "Invalid usage: chp_receiver CONSTELLATION_GROUP" << std::endl;
    }

    auto chirp_manager = chirp::Manager("255.255.255.255", "0.0.0.0", argv[1], "chp_receiver");
    chirp_manager.setAsDefaultInstance();
    chirp_manager.start();

    Logger logger {"chp_receiver"};

    auto receive = [&](const CHP1Message& msg) {
        LOG(logger, DEBUG) << msg.getSender() << " reports state " << magic_enum::enum_name(msg.getState())
                           << ", next message in " << msg.getInterval().count();
    };

    HeartbeatRecv receiver {std::bind(receive, std::placeholders::_1)};

    std::stop_token stop;
    receiver.loop(stop);

    return 0;
}
