#pragma once

#include <string_view>

#include "asio.hpp"

#include "CHIRP/config.hpp"

namespace cnstln {
namespace CHIRP {

class BroadcastSend {
public:
    CHIRP_API BroadcastSend(asio::ip::address brd_address = asio::ip::address_v4::any());
    CHIRP_API BroadcastSend(std::string_view brd_ip);

    CHIRP_API void SendBroadcast(std::string_view message);
    CHIRP_API void SendBroadcast(const void* data, std::size_t size);

private:
    asio::io_context io_context_;
    asio::ip::udp::endpoint endpoint_;
    asio::ip::udp::socket socket_;
};

}
}
