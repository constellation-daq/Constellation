#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "asio.hpp"

#include "CHIRP/config.hpp"

namespace cnstln {
namespace CHIRP {

struct BroadcastMessage {
    std::vector<std::uint8_t> content;
    asio::ip::address ip;
    CHIRP_API std::string content_to_string() const;
};

class BroadcastRecv {
public:
    CHIRP_API BroadcastRecv(asio::io_context& io_context, asio::ip::address any_address);
    CHIRP_API BroadcastRecv(asio::io_context& io_context, const std::string& any_ip);
    CHIRP_API BroadcastRecv(asio::io_context& io_context);

    CHIRP_API BroadcastMessage RecvBroadcast();

private:
    asio::ip::udp::endpoint endpoint_;
    asio::ip::udp::socket socket_;
};

}
}
