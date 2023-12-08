/**
 * @file
 * @brief Example implementation of CHIRP broadcast sender
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <iostream>
#include <string>

#include <asio.hpp>

#include "constellation/protocols/CHIRP/BroadcastSend.hpp"

using namespace cnstln::CHIRP;

int main(int argc, char* argv[]) {
    // Specify brd address via cmdline
    asio::ip::address brd_address = asio::ip::address_v4::broadcast();
    if (argc >= 2) {
        brd_address = asio::ip::make_address(argv[1]);
    }

    BroadcastSend sender {brd_address};

    while(true) {
        std::string message;
        std::cout << "send message: ";
        std::getline(std::cin, message);

        sender.SendBroadcast(message);
    }

    return 0;
}
