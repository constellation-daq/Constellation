#include <iostream>
#include <string>

#include "asio.hpp"

#include "CHIRP/BroadcastSend.hpp"

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
