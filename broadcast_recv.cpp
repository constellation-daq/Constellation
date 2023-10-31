#include <iostream>

#include "asio.hpp"

#include "BroadcastRecv.hpp"

int main() {
    asio::io_context io_context {};
    BroadcastRecv receiver {io_context};

    while(true) {
        std::cout << "recv message: " << receiver.RecvBroadcast() << std::endl;
    }
}
