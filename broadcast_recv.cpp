#include <iostream>

#include "asio.hpp"

#include "BroadcastRecv.hpp"

int main() {
    asio::io_context io_context {};
    BroadcastRecv receiver {io_context};

    while(true) {
        auto message = receiver.RecvBroadcast();
        std::cout << "recv message from " << message.ip.to_string() << ": " << message.content << std::endl;
    }
}
