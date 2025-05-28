/**
 * @file
 * @brief CHP heartbeat sender
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <ostream>
#include <span>
#include <string>
#include <utility>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/heartbeat/HeartbeatSend.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"

using namespace constellation;
using namespace constellation::heartbeat;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::chrono_literals;
using namespace std::string_literals;

namespace {
    void cli_loop(std::span<char*> args) {
        // Get group, name and interval via cmdline
        std::cout << "Usage: chp_sender CONSTELLATION_GROUP NAME INTERVAL_MS\n" << std::flush;

        auto group = "constellation"s;
        auto name = "chp_sender"s;
        auto interval = 1000ms;
        if(args.size() >= 2) {
            group = args[1];
        }
        std::cout << "Using constellation group " << std::quoted(group) << "\n" << std::flush;
        if(args.size() >= 3) {
            name = args[2];
        }
        if(args.size() >= 4) {
            interval = std::chrono::milliseconds(std::stoi(args[3]));
        }

        ManagerLocator::getSinkManager().setConsoleLevels(WARNING);

        auto chirp_manager = std::make_unique<chirp::Manager>("255.255.255.255", "0.0.0.0", group, name);
        chirp_manager->start();
        ManagerLocator::setDefaultCHIRPManager(std::move(chirp_manager));

        auto state = CSCP::State::NEW;

        HeartbeatSend sender {std::move(name), [&]() { return state; }, interval};

        while(true) {
            std::cout << "-----------------------------------------\n" << std::flush;
            // Type
            std::string state_s {};
            std::cout << "State:    [" << state << "] ";
            std::getline(std::cin, state_s);
            state = enum_cast<CSCP::State>(state_s).value_or(state);
            sender.sendExtrasystole(state_s);
        }
    }
} // namespace

int main(int argc, char* argv[]) {
    try {
        cli_loop(std::span(argv, argc));
    } catch(...) {
        return 1;
    }
    return 0;
}
