#include <string>
#include <vector>

#include "asio.hpp"

struct BroadcastMessage {
    std::vector<char> content;
    asio::ip::address ip;
    std::string content_to_string() const { return std::string(content.data(), content.size()); }
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
