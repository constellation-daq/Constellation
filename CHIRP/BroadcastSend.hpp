#pragma once

#include <string_view>

#include "asio.hpp"

#include "CHIRP/config.hpp"

namespace cnstln {
namespace CHIRP {

class BroadcastSend {
public:
    CHIRP_API BroadcastSend(asio::io_context& io_context, asio::ip::address broadcast_address);
    CHIRP_API BroadcastSend(asio::io_context& io_context, std::string_view broadcast_ip);
    CHIRP_API BroadcastSend(asio::io_context& io_context);

    CHIRP_API void SendBroadcast(std::string_view message);
    CHIRP_API void SendBroadcast(const void* data, std::size_t size);

private:
    asio::ip::udp::endpoint endpoint_;
    asio::ip::udp::socket socket_;
};

}
}
