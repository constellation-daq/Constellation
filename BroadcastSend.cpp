#include "BroadcastSend.hpp"

BroadcastSend::BroadcastSend(asio::io_context& io_context, asio::ip::address_v4 broadcast_address)
  : io_context_(io_context),
    endpoint_(std::move(broadcast_address), asio::ip::port_type(60123)),
    socket_(io_context, endpoint_.protocol()) {
    // Set reuseable address and broadcast socket options
    socket_.set_option(asio::socket_base::reuse_address(true));
    socket_.set_option(asio::socket_base::broadcast(true));
    // Set broadcast address for use in send() function
    socket_.connect(endpoint_);
}

BroadcastSend::BroadcastSend(asio::io_context& io_context, const std::string& broadcast_ip)
  : BroadcastSend(io_context, asio::ip::address_v4::from_string(broadcast_ip)) {}

BroadcastSend::BroadcastSend(asio::io_context& io_context)
  : BroadcastSend(io_context, asio::ip::address_v4::broadcast()) {}

void BroadcastSend::SendBroadcast(std::string_view message) {
    socket_.send(asio::buffer(message));
}

void BroadcastSend::SendBroadcast(const std::string& message) {
    SendBroadcast(std::string_view(message.data(), message.length()));
}

void BroadcastSend::SendBroadcast(const void* data, std::size_t size) {
    socket_.send(asio::const_buffer(data, size));
}
