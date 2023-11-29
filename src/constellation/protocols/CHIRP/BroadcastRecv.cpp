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

BroadcastRecv::BroadcastRecv(asio::ip::address any_address)
  : io_context_(), endpoint_(std::move(any_address), asio::ip::port_type(CHIRP_PORT)),
    socket_(io_context_, endpoint_.protocol()) {
    // Set reuseable address socket option
    socket_.set_option(asio::socket_base::reuse_address(true));
    // Bind socket on receiving side
    socket_.bind(endpoint_);
}

BroadcastRecv::BroadcastRecv(std::string_view any_ip)
  : BroadcastRecv(asio::ip::make_address(any_ip)) {}

BroadcastMessage BroadcastRecv::RecvBroadcast() {
    BroadcastMessage message {};

    // Reserve some space for message
    message.content.resize(MESSAGE_BUFFER);

    // Receive content and length of message
    asio::ip::udp::endpoint sender_endpoint {};
    auto length = socket_.receive_from(asio::buffer(message.content), sender_endpoint);

    // Store IP address
    message.address = sender_endpoint.address();

    // Resize content to actual message length
    message.content.resize(length);

    return message;
}

std::optional<BroadcastMessage> BroadcastRecv::AsyncRecvBroadcast(std::chrono::steady_clock::duration timeout) {
    BroadcastMessage message {};
    message.content.resize(MESSAGE_BUFFER);
    asio::ip::udp::endpoint sender_endpoint {};

    // Receive as future
    auto length_future = socket_.async_receive_from(asio::buffer(message.content), sender_endpoint, asio::use_future);

    // Run IO context for timeout
    io_context_.restart();
    io_context_.run_for(timeout);

    // If IO context not stopped, then no message received
    if (!io_context_.stopped()) {
        // Cancel async operations
        socket_.cancel();
        return std::nullopt;
    }

    message.address = sender_endpoint.address();
    message.content.resize(length_future.get());
    return message;
}
