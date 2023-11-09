#include <charconv>
#include <iostream>
#include <string>

#include "asio.hpp"
#include "magic_enum.hpp"

#include "CHIRP/BroadcastSend.hpp"
#include "CHIRP/Message.hpp"

using namespace cnstln::CHIRP;

int main(int argc, char* argv[]) {
    // Specify broadcast address via cmdline
    asio::ip::address brd_address = asio::ip::address_v4::broadcast();
    if (argc >= 2) {
        brd_address = asio::ip::make_address(argv[1]);
    }

    BroadcastSend sender {brd_address};

    while (true) {
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
        if (group.size() == 0) {
            group = "cnstln1";
        }
        // Host
        std::string host {};
        std::cout << "Host:    [satname] ";
        std::getline(std::cin, host);
        if (host.size() == 0) {
            host = "satname";
        }
        // Service
        std::string service_s {};
        std::cout << "Service: [CONTROL] ";
        std::getline(std::cin, type_s);
        auto service = magic_enum::enum_cast<ServiceIdentifier>(type_s).value_or(CONTROL);
        // Port
        std::string port_s {};
        std::cout << "Port:    [23999]   ";
        std::getline(std::cin, port_s);
        Port port = 23999;
        std::from_chars(port_s.data(), port_s.data() + port_s.size(), port);

        auto chirp_msg = Message(type, group, host, service, port);
        std::cout << "Group:   " << chirp_msg.GetGroupID().to_string() << std::endl;
        std::cout << "Name:    " << chirp_msg.GetHostID().to_string() << std::endl;

        auto asm_msg = chirp_msg.Assemble();
        sender.SendBroadcast(asm_msg.data(), asm_msg.size());
    }

    return 0;
}
