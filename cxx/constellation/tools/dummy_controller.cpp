/**
 * @file
 * @brief Dummy controller
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <iostream>
#include <memory>
#include <string>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/CSIG_definitions.hpp"
#include "constellation/core/utils/casts.hpp"

using namespace constellation::message;
using namespace constellation::utils;

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
        if(command == "initialize" || command == "reconfigure" || command == "start") {
            // HACK: add payload if initialize, reconfigure, or start
            send_msg.addPayload(std::make_shared<zmq::message_t>("this is a dummy payload"));
            std::cout << "added payload " << send_msg.hasPayload() << std::endl;
        }
        send_msg.assemble().send(req);

        // Receive reply
        zmq::multipart_t recv_zmq_msg {};
        recv_zmq_msg.recv(req);
        auto recv_msg = CSCP1Message::disassemble(recv_zmq_msg);

        // Print message
        std::cout << recv_msg.getHeader().to_string() << "\n"
                  << "Verb: " << to_string(recv_msg.getVerb().first) << " : " << recv_msg.getVerb().second << std::endl;
    }
}
