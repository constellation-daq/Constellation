/**
 * @file
 * @brief Dummy controller
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono> // IWYU pragma: keep
#include <iomanip>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include <msgpack.hpp>

#include "constellation/controller/Controller.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/networking/asio_helpers.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"

using namespace constellation;
using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::networking;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::chrono_literals;
using namespace std::string_literals;

namespace {
    void cli_loop(std::span<char*> args) {
        // Get the default logger
        auto& logger = Logger::getDefault();
        ManagerLocator::getSinkManager().setConsoleLevels(INFO);
        LOG(logger, STATUS) << "Usage: dummy_controller CONSTELLATION_GROUP";

        // Get group via cmdline
        auto group = "constellation"s;
        if(args.size() >= 2) {
            group = args[1];
        }
        LOG(logger, STATUS) << "Using constellation group " << std::quoted(group);

        const std::string name = "dummy_controller";

        auto chirp_manager = std::make_unique<chirp::Manager>(group, name, get_interfaces());
        chirp_manager->start();
        chirp_manager->sendRequest(CHIRP::ServiceIdentifier::CONTROL);
        ManagerLocator::setDefaultCHIRPManager(std::move(chirp_manager));

        LOG(logger, STATUS) << "Starting controller \"" << name << "\"";
        Controller controller {name};
        controller.start();

        while(true) {
            // Flush logger before printing to cout
            logger.flush();

            std::string command {};
            std::cout << "\nCommand: ";
            std::getline(std::cin, command);

            // Avoid sending infinite loop of empty commands on Ctrl+D
            if(command.empty()) {
                continue;
            }

            // Quit if requested
            if(command == "quit") {
                break;
            }

            // Send command to all satellites we currently know
            auto send_msg = CSCP1Message({"dummy_controller"}, {CSCP1Message::Type::REQUEST, command});
            if(command == "initialize" || command == "reconfigure") {
                send_msg.addPayload(Dictionary().assemble());
                LOG(logger, DEBUG) << "Added empty configuration to message";
            } else if(command == "start") {
                const std::string run_identifier = "1234";
                msgpack::sbuffer sbuf {};
                msgpack::pack(sbuf, run_identifier);
                send_msg.addPayload(std::move(sbuf));
                LOG(logger, DEBUG) << "Added run identifier \"" << run_identifier << "\" to message";
            }

            auto responses = controller.sendCommands(send_msg);
            for(const auto& [sat, recv_msg] : responses) {
                // Print message
                LOG(logger, INFO) << recv_msg.getHeader().to_string() << "\n"
                                  << "Verb: " << recv_msg.getVerb().first << " : " << recv_msg.getVerb().second;

                // Print payload if dict
                if(recv_msg.hasPayload()) {
                    try {
                        const auto dict = Dictionary::disassemble(recv_msg.getPayload());
                        if(!dict.empty()) {
                            LOG(logger, INFO) << dict.to_string();
                        }
                    } catch(...) {
                        LOG(logger, WARNING) << "Payload: <could not unpack payload>";
                    }
                }
            }
        }

        controller.stop();
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
