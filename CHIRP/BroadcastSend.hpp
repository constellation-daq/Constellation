#pragma once

#include <string_view>

#include "asio.hpp"

namespace cnstln {
namespace CHIRP {

class BroadcastSend {
public:
    BroadcastSend(asio::io_context& io_context, asio::ip::address broadcast_address);
    BroadcastSend(asio::io_context& io_context, std::string_view broadcast_ip);
    BroadcastSend(asio::io_context& io_context);

    void SendBroadcast(std::string_view message);
    void SendBroadcast(const void* data, std::size_t size);

private:
    asio::io_context& io_context_;
    asio::ip::udp::endpoint endpoint_;
    asio::ip::udp::socket socket_;
};

}
}
