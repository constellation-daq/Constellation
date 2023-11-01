#include "BroadcastRecv.hpp"

BroadcastRecv::BroadcastRecv(asio::io_context& io_context)
  : io_context_(io_context),
    endpoint_(asio::ip::address_v4::any(), asio::ip::port_type(60123)),
    socket_(io_context, endpoint_.protocol()) {
    // Set reuseable address and broadcast option
    socket_.set_option(asio::socket_base::reuse_address(true));
    socket_.set_option(asio::socket_base::broadcast(true));
    // Bind socket on receiving side
    socket_.bind(endpoint_);
}

std::string BroadcastRecv::RecvBroadcast() {
    // Create string and reserve some space
    std::string message;
    message.resize(1024);
    // Receive data and length of message
    auto length = socket_.receive_from(asio::buffer(message), endpoint_);
    // Resize string to actual message length
    message.resize(length);
    return message;
}
