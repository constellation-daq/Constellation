#include "BroadcastRecv.hpp"

BroadcastRecv::BroadcastRecv(asio::io_context& io_context)
  : io_context_(io_context),
    endpoint_(asio::ip::address_v4::any(), asio::ip::port_type(60123)),
    socket_(io_context, endpoint_.protocol()) {
    // Set reuseable address and broadcast socket options
    socket_.set_option(asio::socket_base::reuse_address(true));
    socket_.set_option(asio::socket_base::broadcast(true));
    // Bind socket on receiving side
    socket_.bind(endpoint_);
}

BroadcastMessage BroadcastRecv::RecvBroadcast() {
    BroadcastMessage message {};

    // Reserve some space for message
    message.content.resize(1024);

    // Receive content and length of message
    asio::ip::udp::endpoint sender_endpoint {};
    auto length = socket_.receive_from(asio::buffer(message.content), sender_endpoint);

    // Store IP address
    message.ip = sender_endpoint.address();

    // Resize content to actual message length
    message.content.resize(length);

    return message;
}
