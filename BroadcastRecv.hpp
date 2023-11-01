#include <string>

#include "asio.hpp"

struct BroadcastMessage {
    std::string content;
    asio::ip::address ip;
};

class BroadcastRecv {
public:
    BroadcastRecv(asio::io_context& io_context);

    BroadcastMessage RecvBroadcast();

private:
    asio::io_context& io_context_;
    asio::ip::udp::endpoint endpoint_;
    asio::ip::udp::socket socket_;
};
