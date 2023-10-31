#include <iostream>
#include <string>

#include "asio.hpp"

#include "BroadcastSend.hpp"

int main() {
    asio::io_context io_context {};
    BroadcastSend sender {io_context};

    while(true) {
        std::string message;
        std::cout << "send message: ";
        std::cin >> message;

        sender.SendBroadcast(message);
    }
}
