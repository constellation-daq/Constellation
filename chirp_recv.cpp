#include <iostream>

#include "magic_enum.hpp"

#include "BroadcastRecv.hpp"
#include "CHIRP/Message.hpp"

using namespace CHIRP;

int main() {
    auto io_context = asio::io_context();
    auto broadcast_recver = BroadcastRecv(io_context);

    while(true) {
        // Receive message
        auto brd_msg = broadcast_recver.RecvBroadcast();

        // Build message from message
        auto msg = Message(brd_msg.content);

        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Type:    " << magic_enum::enum_name(msg.GetType()) << std::endl;
        std::cout << "Group:   " << msg.GetGroupHash().toString() << std::endl;
        std::cout << "Name:    " << msg.GetNameHash().toString() << std::endl;
        std::cout << "Service: " << magic_enum::enum_name(msg.GetServiceIdentifier()) << std::endl;
        std::cout << "Port:    " << msg.GetPort() << std::endl;
    }
}
