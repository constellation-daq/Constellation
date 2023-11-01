#include <string_view>
#include <string>

#include "asio.hpp"

class BroadcastSend {
public:
    BroadcastSend(asio::io_context& io_context, asio::ip::address_v4 broadcast_address);
    BroadcastSend(asio::io_context& io_context, const std::string& broadcast_ip);
    BroadcastSend(asio::io_context& io_context);

    void SendBroadcast(std::string_view message);
    void SendBroadcast(const std::string& message);

private:
    asio::io_context& io_context_;
    asio::ip::udp::endpoint endpoint_;
    asio::ip::udp::socket socket_;
};
