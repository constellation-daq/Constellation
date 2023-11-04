#include <iostream>
#include <string>
#include <vector>

#include "asio.hpp"

#include "CHIRP/BroadcastSend.hpp"
#include "CHIRP/BroadcastRecv.hpp"

using namespace cnstln::CHIRP;
using namespace std::literals::string_literals;

int test_broadcast_send_recv_string(asio::io_context& io_context) {
    BroadcastRecv receiver {io_context};
    BroadcastSend sender {io_context};

    // Start receiving new message
    auto msg_future = std::async(&BroadcastRecv::RecvBroadcast, &receiver);
    // Send message (string)
    auto msg_content = "test message"s;
    sender.SendBroadcast(msg_content);
    // Receive message
    auto msg = msg_future.get();
    // Convert to string and compare
    return (msg.content_to_string() == msg_content) ? 0 : 1;
}

int test_broadcast_send_recv_array(asio::io_context& io_context) {
    BroadcastRecv receiver {io_context, "0.0.0.0"};
    BroadcastSend sender {io_context, "255.255.255.255"};

    // Start receiving new message
    auto msg_future = std::async(&BroadcastRecv::RecvBroadcast, &receiver);
    // Send message (bytes)
    auto msg_content = std::vector<std::uint8_t>({'T', 'E', 'S', 'T'});
    sender.SendBroadcast(msg_content.data(), msg_content.size());
    // Receive message
    auto msg = msg_future.get();
    // Compare
    return (msg.content == msg_content) ? 0 : 1;
}

int main() {
    asio::io_context io_context {};

    int ret = 0;
    int ret_test = 0;

    // test_broadcast_send_recv_string
    std::cout << "test_broadcast_send_recv_string...           " << std::flush;
    ret_test = test_broadcast_send_recv_string(io_context);
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // broadcast_send_recv_array
    std::cout << "test_broadcast_send_recv_array...            " << std::flush;
    ret_test = test_broadcast_send_recv_array(io_context);
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    if (ret == 0) {
        std::cout << "\nAll tests passed" << std::endl;
    }
    else {
        std::cout << "\n" << ret << " tests failed" << std::endl;
    }
    return ret;
}
