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

#include "constellation/protocols/CHIRP/BroadcastRecv.hpp"

using namespace cnstln::CHIRP;

int main(int argc, char* argv[]) {
    // Specify any address via cmdline
    asio::ip::address any_address = asio::ip::address_v4::any();
    if(argc >= 2) {
        any_address = asio::ip::make_address(argv[1]);
    }

    BroadcastRecv receiver {any_address};

    while(true) {
        auto message = receiver.RecvBroadcast();
        std::cout << "recv message from " << message.address.to_string() << ": " << message.content_to_string() << std::endl;
    }

    return 0;
}
