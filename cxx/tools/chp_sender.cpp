/**
 * @file
 * @brief CHP heartbeat sender
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <iostream>
#include <span>
#include <string>

#include <magic_enum.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/heartbeat/HeartbeatSend.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation;
using namespace constellation::heartbeat;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::chrono_literals;
using namespace std::literals::string_literals;

void cli_loop(std::span<char*> args) {
    // Get group, name and interval via cmdline
    std::cout << "Usage: chp_sender CONSTELLATION_GROUP NAME INTERVAL_MS" << std::endl;

    auto group = "constellation"s;
    auto name = "chp_sender"s;
    auto interval = 1000ms;
    if(args.size() >= 2) {
        group = args[1];
    }
    std::cout << "Using constellation group " << std::quoted(group) << std::endl;
    if(args.size() >= 3) {
        name = args[2];
    }
    if(args.size() >= 4) {
        interval = std::chrono::milliseconds(std::stoi(args[3]));
    }

    auto chirp_manager = chirp::Manager("255.255.255.255", "0.0.0.0", group, name);
    chirp_manager.setAsDefaultInstance();
    chirp_manager.start();

    HeartbeatSend sender {name, interval};

    auto state = State::NEW;
    while(true) {
        std::cout << "-----------------------------------------" << std::endl;
        // Type
        std::string state_s {};
        std::cout << "State:    [" << to_string(state) << "] ";
        std::getline(std::cin, state_s);
        state = magic_enum::enum_cast<State>(state_s, magic_enum::case_insensitive).value_or(state);
        sender.updateState(state);
    }
}

int main(int argc, char* argv[]) {
    try {
        cli_loop(std::span(argv, argc));
    } catch(...) {
        return 1;
    }
    return 0;
}
