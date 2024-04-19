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

using namespace constellation;
using namespace constellation::heartbeat;
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

    HeartbeatRecv receiver {};

    receiver.main_loop();

    return 0;
}
