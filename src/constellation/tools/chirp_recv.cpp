/**
 * @file
 * @brief Example implementation of CHIRP broadcast receiver
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <iomanip>
#include <iostream>

#include <asio.hpp>

#include "constellation/core/chirp/BroadcastRecv.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/casts.hpp"

using namespace constellation::chirp;
using namespace constellation::message;
using namespace constellation::utils;

int main(int argc, char* argv[]) {
    // Specify any address via cmdline
    asio::ip::address any_address = asio::ip::address_v4::any();
    if(argc >= 2) {
        try {
            any_address = asio::ip::make_address(argv[1]);
        } catch(const asio::system_error& error) {
            std::cerr << "Unable to use specified any address " << std::quoted(argv[1]) << ", using default instead"
                      << std::endl;
        }
    }

    BroadcastRecv receiver {any_address, CHIRP_PORT};

    while(true) {
        // Receive message
        auto brd_msg = receiver.recvBroadcast();

        try {
            // Build message from message
            auto chirp_msg = CHIRPMessage::disassemble(brd_msg.content);

            std::cout << "-----------------------------------------" << std::endl;
            std::cout << "Type:    " << to_string(chirp_msg.getType()) << std::endl;
            std::cout << "Group:   " << chirp_msg.getGroupID().to_string() << std::endl;
            std::cout << "Host:    " << chirp_msg.getHostID().to_string() << std::endl;
            std::cout << "Service: " << to_string(chirp_msg.getServiceIdentifier()) << std::endl;
            std::cout << "Port:    " << chirp_msg.getPort() << std::endl;
        } catch(const MessageDecodingError& error) {
            std::cerr << "-----------------------------------------" << std::endl;
            std::cerr << "Received invalid message: " << error.what() << std::endl;
        }
    }

    return 0;
}
