#include <string>

#include "asio.hpp"

class BroadcastRecv {
public:
    BroadcastRecv(asio::io_context& io_context);

    std::string RecvBroadcast();

private:
    asio::io_context& io_context_;
    asio::ip::udp::endpoint endpoint_;
    asio::ip::udp::socket socket_;
};
