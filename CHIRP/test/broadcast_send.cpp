#include <iostream>
#include <string>

#include "asio.hpp"

#include "CHIRP/BroadcastSend.hpp"

using namespace cnstln::CHIRP;

int main(int argc, char* argv[]) {
    // Specify broadcast address via cmdline
    asio::ip::address broadcast_address {};
    if(argc < 2) {
        broadcast_address = asio::ip::address_v4::broadcast();
    }
    else if (argc == 2) {
        broadcast_address = asio::ip::make_address(argv[1]);
    }
    else {
        return 1;
    }

    asio::io_context io_context {};
    BroadcastSend sender {io_context, broadcast_address};

    while(true) {
        std::string message;
        std::cout << "send message: ";
        std::getline(std::cin, message);

        sender.SendBroadcast(message);
    }

    return 0;
}
