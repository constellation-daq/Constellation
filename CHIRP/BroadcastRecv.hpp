#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "asio.hpp"

namespace cnstln {
namespace CHIRP {

struct BroadcastMessage {
    std::vector<std::uint8_t> content;
    asio::ip::address ip;
    std::string content_to_string() const;
};

class BroadcastRecv {
public:
    BroadcastRecv(asio::io_context& io_context, asio::ip::address any_address);
    BroadcastRecv(asio::io_context& io_context, const std::string& any_ip);
    BroadcastRecv(asio::io_context& io_context);

    BroadcastMessage RecvBroadcast();

private:
    asio::io_context& io_context_;
    asio::ip::udp::endpoint endpoint_;
    asio::ip::udp::socket socket_;
};

}
}
