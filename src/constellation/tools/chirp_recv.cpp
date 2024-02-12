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
#include <magic_enum.hpp>

#include "constellation/chirp/BroadcastRecv.hpp"
#include "constellation/chirp/exceptions.hpp"
#include "constellation/chirp/Message.hpp"
#include "constellation/chirp/protocol_info.hpp"

using namespace constellation::chirp;

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
        auto brd_msg = receiver.RecvBroadcast();

        try {
            // Build message from message
            auto chirp_msg = Message(brd_msg.content);

            std::cout << "-----------------------------------------" << std::endl;
            std::cout << "Type:    " << magic_enum::enum_name(chirp_msg.GetType()) << std::endl;
            std::cout << "Group:   " << chirp_msg.GetGroupID().to_string() << std::endl;
            std::cout << "Host:    " << chirp_msg.GetHostID().to_string() << std::endl;
            std::cout << "Service: " << magic_enum::enum_name(chirp_msg.GetServiceIdentifier()) << std::endl;
            std::cout << "Port:    " << chirp_msg.GetPort() << std::endl;
        } catch(const DecodeError& error) {
            std::cerr << "-----------------------------------------" << std::endl;
            std::cerr << "Received invalid message: " << error.what() << std::endl;
        }
    }

    return 0;
}
