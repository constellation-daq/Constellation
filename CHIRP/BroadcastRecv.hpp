#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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
    CHIRP_API BroadcastRecv(asio::ip::address any_address = asio::ip::address_v4::any());
    CHIRP_API BroadcastRecv(std::string_view any_ip);

    CHIRP_API BroadcastMessage RecvBroadcast();
    CHIRP_API std::optional<BroadcastMessage> AsyncRecvBroadcast(std::chrono::steady_clock::duration timeout);

private:
    asio::io_context io_context_;
    asio::ip::udp::endpoint endpoint_;
    asio::ip::udp::socket socket_;
};

}
}
