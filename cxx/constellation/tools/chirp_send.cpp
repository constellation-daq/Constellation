/**
 * @file
 * @brief Example implementation of CHIRP broadcast sender
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <charconv>
#include <iomanip>
#include <iostream>
#include <string>

#include <asio.hpp>
#include <magic_enum.hpp>

#include "constellation/core/chirp/BroadcastSend.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/utils/ports.hpp"

using namespace constellation::chirp;
using namespace constellation::message;
using namespace constellation::utils;

int main(int argc, char* argv[]) {
    // Specify broadcast address via cmdline
    asio::ip::address brd_address = asio::ip::address_v4::broadcast();
    if(argc >= 2) {
        try {
            brd_address = asio::ip::make_address(argv[1]);
        } catch(const asio::system_error& error) {
            std::cerr << "Unable to use specified broadcast address " << std::quoted(argv[1]) << ", using default instead"
                      << std::endl;
        }
    }

    BroadcastSend sender {brd_address, CHIRP_PORT};

    while(true) {
        std::cout << "-----------------------------------------" << std::endl;
        // Type
        std::string type_s {};
        std::cout << "Type:    [REQUEST] ";
        std::getline(std::cin, type_s);
        auto type = magic_enum::enum_cast<MessageType>(type_s).value_or(REQUEST);
        // Group
        std::string group {};
        std::cout << "Group:   [cnstln1] ";
        std::getline(std::cin, group);
        if(group.empty()) {
            group = "cnstln1";
        }
        // Host
        std::string host {};
        std::cout << "Host:    [satname] ";
        std::getline(std::cin, host);
        if(host.empty()) {
            host = "satname";
        }
        // Service
        std::string service_s {};
        std::cout << "Service: [CONTROL] ";
        std::getline(std::cin, service_s);
        auto service = magic_enum::enum_cast<ServiceIdentifier>(service_s).value_or(CONTROL);
        // Port
        std::string port_s {};
        std::cout << "Port:    [23999]   ";
        std::getline(std::cin, port_s);
        Port port = 23999;
        std::from_chars(port_s.data(), port_s.data() + port_s.size(), port);

        auto chirp_msg = CHIRPMessage(type, group, host, service, port);
        std::cout << "Group:   " << chirp_msg.getGroupID().to_string() << std::endl;
        std::cout << "Name:    " << chirp_msg.getHostID().to_string() << std::endl;

        auto asm_msg = chirp_msg.assemble();
        sender.sendBroadcast(asm_msg);
    }

    return 0;
}
