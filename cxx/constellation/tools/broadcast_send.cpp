/**
 * @file
 * @brief Example implementation of CHIRP broadcast sender
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <charconv>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

#include <asio.hpp>

#include "constellation/core/chirp/BroadcastSend.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"

using namespace constellation::chirp;

int main(int argc, char* argv[]) {
    // Specify brd address via cmdline
    asio::ip::address brd_address = asio::ip::address_v4::broadcast();
    asio::ip::port_type port = CHIRP_PORT;
    if(argc >= 2) {
        try {
            brd_address = asio::ip::make_address(argv[1]);
        } catch(const asio::system_error& error) {
            std::cerr << "Unable to use specified broadcast address " << std::quoted(argv[1]) << ", using default instead"
                      << std::endl;
        }
    }
    if(argc >= 3) {
        std::from_chars(argv[2], argv[2] + std::strlen(argv[2]), port);
    }

    BroadcastSend sender {brd_address, port};

    while(true) {
        std::string message;
        std::cout << "Send message: ";
        std::getline(std::cin, message);

        sender.sendBroadcast(message);
    }

    return 0;
}
