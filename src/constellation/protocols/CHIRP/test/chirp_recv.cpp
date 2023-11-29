/**
 * @file
 * @brief Example implementation of CHIRP broadcast receiver
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <iostream>

#include <asio.hpp>
#include <magic_enum.hpp>

#include "constellation/protocols/CHIRP/BroadcastRecv.hpp"
#include "constellation/protocols/CHIRP/Message.hpp"

using namespace cnstln::CHIRP;

int main(int argc, char* argv[]) {
    // Specify any address via cmdline
    asio::ip::address any_address = asio::ip::address_v4::any();
    if(argc >= 2) {
        any_address = asio::ip::make_address(argv[1]);
    }

    BroadcastRecv receiver {any_address};

    while(true) {
        // Receive message
        auto brd_msg = receiver.RecvBroadcast();

        // Build message from message
        auto chirp_msg = Message(AssembledMessage(brd_msg.content));

        std::cout << "-----------------------------------------" << std::endl;
        std::cout << "Type:    " << magic_enum::enum_name(chirp_msg.GetType()) << std::endl;
        std::cout << "Group:   " << chirp_msg.GetGroupID().to_string() << std::endl;
        std::cout << "Host:    " << chirp_msg.GetHostID().to_string() << std::endl;
        std::cout << "Service: " << magic_enum::enum_name(chirp_msg.GetServiceIdentifier()) << std::endl;
        std::cout << "Port:    " << chirp_msg.GetPort() << std::endl;
    }

    return 0;
}
