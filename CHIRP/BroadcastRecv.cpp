#include "BroadcastRecv.hpp"

#include "CHIRP/protocol_info.hpp"

using namespace cnstln::CHIRP;

constexpr std::size_t MESSAGE_BUFFER = 1024;

std::string BroadcastMessage::content_to_string() const {
    std::string ret;
    ret.resize(content.size());
    for(std::size_t n = 0; n < content.size(); ++n) {
        ret.at(n) = static_cast<char>(content[n]);
    }
    return ret;
}

BroadcastRecv::BroadcastRecv(asio::io_context& io_context, asio::ip::address any_address)
  : io_context_(io_context),
    endpoint_(any_address, asio::ip::port_type(CHIRP_PORT)),
    socket_(io_context, endpoint_.protocol()) {
    // Set reuseable address and broadcast socket options
    socket_.set_option(asio::socket_base::reuse_address(true));
    socket_.set_option(asio::socket_base::broadcast(true));
    // Bind socket on receiving side
    socket_.bind(endpoint_);
}

BroadcastRecv::BroadcastRecv(asio::io_context& io_context, const std::string& any_ip)
  : BroadcastRecv(io_context, asio::ip::make_address(any_ip)) {}

BroadcastRecv::BroadcastRecv(asio::io_context& io_context)
  : BroadcastRecv(io_context, asio::ip::address_v4::any()) {}

BroadcastMessage BroadcastRecv::RecvBroadcast() {
    BroadcastMessage message {};

    // Reserve some space for message
    message.content.resize(MESSAGE_BUFFER);

    // Receive content and length of message
    asio::ip::udp::endpoint sender_endpoint {};
    auto length = socket_.receive_from(asio::buffer(message.content), sender_endpoint);

    // Store IP address
    message.ip = sender_endpoint.address();

    // Resize content to actual message length
    message.content.resize(length);

    return message;
}
