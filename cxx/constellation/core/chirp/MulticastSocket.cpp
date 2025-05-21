/**
 * @file
 * @brief Implementation of the multicast socket
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MulticastSocket.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <set>
#include <utility>

#include <asio.hpp>

using namespace constellation::chirp;

constexpr std::size_t MESSAGE_BUFFER = 1024;
constexpr int MULTICAST_TTL = 8;

MulticastSocket::MulticastSocket(std::set<asio::ip::address_v4> interface_addresses,
                                 const asio::ip::address_v4& multicast_address,
                                 asio::ip::port_type multicast_port)
    : socket_(io_context_), interface_addresses_(std::move(interface_addresses)),
      multicast_endpoint_(multicast_address, multicast_port) {

    // Open socket to set protocol
    socket_.open(multicast_endpoint_.protocol());

    // Ensure socket can be bound by other programs
    socket_.set_option(asio::ip::udp::socket::reuse_address(true));

    // Set Multicast TTL (aka network hops)
    socket_.set_option(asio::ip::multicast::hops(MULTICAST_TTL));

    // Disable loopback since loopback interface is added explicitly
    socket_.set_option(asio::ip::multicast::enable_loopback(false));

    // Bind socket
    socket_.bind(multicast_endpoint_);

    // Join multicast group on each interface
    for(const auto& interface_address : interface_addresses_) {
        socket_.set_option(asio::ip::multicast::join_group(multicast_address, interface_address));
    }
}

void MulticastSocket::sendMessage(std::span<const std::byte> message, const asio::ip::address_v4& interface_address) {
    socket_.set_option(asio::ip::multicast::outbound_interface(interface_address));
    socket_.send_to(asio::const_buffer(message.data(), message.size()), multicast_endpoint_);
}

void MulticastSocket::sendMessage(std::span<const std::byte> message) {
    std::ranges::for_each(interface_addresses_, [&](auto& interface_address) { sendMessage(message, interface_address); });
}

std::optional<MulticastMessage> MulticastSocket::recvMessage(std::chrono::steady_clock::duration timeout) {
    MulticastMessage message {};
    message.content.resize(MESSAGE_BUFFER);
    asio::ip::udp::endpoint sender_endpoint {};

    // Receive as future
    auto length_future = socket_.async_receive_from(asio::buffer(message.content), sender_endpoint, asio::use_future);

    // Run IO context for timeout
    io_context_.restart();
    io_context_.run_for(timeout);

    // If IO context not stopped, then no message received
    if(!io_context_.stopped()) {
        // Cancel async operations
        socket_.cancel();
        return std::nullopt;
    }

    message.address = sender_endpoint.address().to_v4();
    message.content.resize(length_future.get());
    return message;
}
