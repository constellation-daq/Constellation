/**
 * @file
 * @brief CHP heartbeat sender
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <functional>
#include <iostream>
#include <string>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/heartbeat/HeartbeatSend.hpp"

using namespace constellation;
using namespace constellation::heartbeat;
using namespace constellation::message;
using namespace std::literals::chrono_literals;

// Use global std::function to work around C linkage
std::function<void(int)> signal_handler_f {}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" void signal_hander(int signal) {
    signal_handler_f(signal);
}

int main(int argc, char* argv[]) {
    // Get address via cmdline
    if(argc != 4) {
        std::cout << "Invalid usage: chp_sender CONSTELLATION_GROUP SENDER_NAME INTERVAL_MS" << std::endl;
    }

    auto chirp_manager = chirp::Manager("255.255.255.255", "0.0.0.0", argv[1], "chp_sender");
    chirp_manager.setAsDefaultInstance();
    chirp_manager.start();

    auto interval = std::chrono::milliseconds(std::stoi(argv[3]));
    const HeartbeatSend sender {argv[2], interval};

    // FIXME better solution to wait until we interrupt?
    std::this_thread::sleep_for(500s);

    return 0;
}
