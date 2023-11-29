#include <chrono>
#include <iostream>
#include <future>
#include <string>
#include <vector>

#include "asio.hpp"

#include "CHIRP/BroadcastSend.hpp"
#include "CHIRP/BroadcastRecv.hpp"

using namespace cnstln::CHIRP;
using namespace std::literals::chrono_literals;
using namespace std::literals::string_literals;

int test_broadcast_send_recv_string() {
    BroadcastRecv receiver {"0.0.0.0"};
    BroadcastSend sender {"0.0.0.0"};

    // Start receiving new message
    auto msg_future = std::async(&BroadcastRecv::RecvBroadcast, &receiver);
    // Send message (string)
    auto msg_content = "test message"s;
    sender.SendBroadcast(msg_content);
    // Receive message
    auto msg = msg_future.get();
    // Convert to string and compare
    return msg.content_to_string() == msg_content ? 0 : 1;
}

int test_broadcast_send_recv_array() {
    BroadcastRecv receiver {"0.0.0.0"};
    BroadcastSend sender {"0.0.0.0"};

    // Start receiving new message
    auto msg_future = std::async(&BroadcastRecv::RecvBroadcast, &receiver);
    // Send message (bytes)
    auto msg_content = std::vector<std::uint8_t>({'T', 'E', 'S', 'T'});
    sender.SendBroadcast(msg_content.data(), msg_content.size());
    // Receive message
    auto msg = msg_future.get();
    // Compare
    return msg.content == msg_content ? 0 : 1;
}

int test_broadcast_localhost_ip() {
    BroadcastRecv receiver {"0.0.0.0"};
    BroadcastSend sender {"0.0.0.0"};

    // Start receiving new message
    auto msg_future = std::async(&BroadcastRecv::RecvBroadcast, &receiver);
    // Send message
    auto msg_content = "test message"s;
    sender.SendBroadcast(msg_content);
    // Receive message
    auto msg = msg_future.get();
    // Compare address
    return msg.address == asio::ip::make_address("0.0.0.0");
}

int test_broadcast_send_async_recv() {
    BroadcastRecv receiver {"0.0.0.0"};
    BroadcastSend sender {"0.0.0.0"};

    // Try receiving new message
    auto msg_opt_future = std::async(&BroadcastRecv::AsyncRecvBroadcast, &receiver, 10ms);
    // Send message
    auto msg_content = "test message"s;
    sender.SendBroadcast(msg_content);
    // Receive message
    auto msg_opt = msg_opt_future.get();
    // Check that a message was returneed
    if (!msg_opt.has_value()) {
        return 1;
    }
    // Check that message is correct
    return msg_opt.value().content_to_string() == msg_content ? 0 : 1;
}

int test_broadcast_async_recv_timeout() {
    BroadcastRecv receiver {"0.0.0.0"};

    // Try receiving new message
    auto msg_opt = receiver.AsyncRecvBroadcast(10ms);

    // No message send, thus check for a timeout
    return msg_opt.has_value() ? 1 : 0;
}

int main() {
    int ret = 0;
    int ret_test = 0;

    // test_broadcast_send_recv_string
    std::cout << "test_broadcast_send_recv_string...           " << std::flush;
    ret_test = test_broadcast_send_recv_string();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_broadcast_send_recv_array
    std::cout << "test_broadcast_send_recv_array...            " << std::flush;
    ret_test = test_broadcast_send_recv_array();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_broadcast_localhost_ip
    std::cout << "test_broadcast_localhost_ip...               " << std::flush;
    ret_test = test_broadcast_localhost_ip();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_broadcast_send_async_recv
    std::cout << "test_broadcast_send_async_recv...            " << std::flush;
    ret_test = test_broadcast_send_async_recv();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_broadcast_async_recv_timeout
    std::cout << "test_broadcast_async_recv_timeout...         " << std::flush;
    ret_test = test_broadcast_async_recv_timeout();
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
