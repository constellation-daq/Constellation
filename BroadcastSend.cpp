#include "BroadcastSend.hpp"

BroadcastSend::BroadcastSend(asio::io_context& io_context)
  : io_context_(io_context),
    endpoint_(asio::ip::address_v4::broadcast(), asio::ip::port_type(60123)),
    socket_(io_context, endpoint_.protocol()) {
    // Set reuseable address and broadcast option
    socket_.set_option(asio::socket_base::reuse_address(true));
    socket_.set_option(asio::socket_base::broadcast(true));
    socket_.bind(endpoint_);
}

void BroadcastSend::SendBroadcast(std::string_view message) {
    socket_.send_to(asio::buffer(message), endpoint_);
}

void BroadcastSend::SendBroadcast(const std::string& message) {
    SendBroadcast(std::string_view(message.data(), message.length()));
};
