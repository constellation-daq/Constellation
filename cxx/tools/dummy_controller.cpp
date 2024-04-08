/**
 * @file
 * @brief Dummy controller
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/satellite_definitions.hpp"
#include "constellation/core/utils/casts.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::utils;

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[]) {
    // Get address via cmdline
    if(argc != 2) {
        std::cout << "Invalid usage: dummy_controller ZMQ_ENDPOINT" << std::endl;
    }

    zmq::context_t context {};
    zmq::socket_t req {context, zmq::socket_type::req};

    req.connect(argv[1]);

    while(true) {
        std::string command;
        std::cout << "Send command: ";
        std::getline(std::cin, command);

        // Send command
        auto send_msg = CSCP1Message({"dummy_controller"}, {CSCP1Message::Type::REQUEST, command});
        if(command == "initialize" || command == "reconfigure") {
            send_msg.addPayload(Configuration().assemble());
            std::cout << "Added empty configuration to message" << std::endl;
        } else if(command == "start") {
            const std::uint32_t run_nr = 1234U;
            msgpack::sbuffer sbuf {};
            msgpack::pack(sbuf, run_nr);
            send_msg.addPayload(std::make_shared<zmq::message_t>(sbuf.data(), sbuf.size()));
            std::cout << "Added run number " << run_nr << " to message" << std::endl;
        }
        send_msg.assemble().send(req);

        // Receive reply
        zmq::multipart_t recv_zmq_msg {};
        recv_zmq_msg.recv(req);
        auto recv_msg = CSCP1Message::disassemble(recv_zmq_msg);

        // Print message
        std::cout << recv_msg.getHeader().to_string() << "\n"
                  << "Verb: " << to_string(recv_msg.getVerb().first) << " : " << recv_msg.getVerb().second << std::endl;

        // Print payload if dict
        if(recv_msg.hasPayload()) {
            try {
                const auto dict = msgpack::unpack(to_char_ptr(recv_msg.getPayload()->data()), recv_msg.getPayload()->size())
                                      ->as<Dictionary>();
                if(!dict.empty()) {
                    std::cout << "Payload:";
                    for(const auto& [key, value] : dict) {
                        std::cout << "\n " << key << ": " << value.str();
                    }
                    std::cout << std::endl;
                }
            } catch(...) {
                std::cout << "Payload: <could not unpack payload>" << std::endl;
            }
        }
    }
}
