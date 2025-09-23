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
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <asio.hpp>

#include "constellation/core/networking/asio_helpers.hpp"

using namespace constellation::chirp;
using namespace constellation::networking;

constexpr std::size_t MESSAGE_BUFFER = 1024;
constexpr int MULTICAST_TTL = 8;

MulticastSocket::MulticastSocket(const std::vector<Interface>& interfaces,
                                 const asio::ip::address_v4& multicast_address,
                                 asio::ip::port_type multicast_port)
    : recv_socket_(io_context_), multicast_endpoint_(multicast_address, multicast_port) {

    // Receive endpoint using any address and multicast port
    const asio::ip::udp::endpoint recv_endpoint {asio::ip::address_v4::any(), multicast_port};

    // Create send sockets
    send_sockets_.reserve(interfaces.size());
    for(const auto& interface : interfaces) {
        asio::ip::udp::socket socket {io_context_};

        // Open socket to set protocol
        socket.open(multicast_endpoint_.protocol());

        // Ensure socket can be bound by other programs
        socket.set_option(asio::ip::udp::socket::reuse_address(true));

        // Set Multicast TTL (aka network hops)
        socket.set_option(asio::ip::multicast::hops(MULTICAST_TTL));

        // Disable loopback since loopback interface is added explicitly
        socket.set_option(asio::ip::multicast::enable_loopback(interface.address.is_loopback()));

        // Set interface address
        socket.set_option(asio::ip::multicast::outbound_interface(interface.address));

        send_sockets_.emplace_back(std::move(socket));
    }

    // Open receive socket to set protocol
    recv_socket_.open(multicast_endpoint_.protocol());

    // Ensure socket can be bound by other programs
    recv_socket_.set_option(asio::ip::udp::socket::reuse_address(true));

    // Set Multicast TTL (aka network hops)
    recv_socket_.set_option(asio::ip::multicast::hops(MULTICAST_TTL));

    // Enable loopback
    recv_socket_.set_option(asio::ip::multicast::enable_loopback(true));

    // Bind socket
    recv_socket_.bind(recv_endpoint);

    // Join multicast group on each interface
    for(const auto& interface : interfaces) {
        recv_socket_.set_option(asio::ip::multicast::join_group(multicast_address, interface.address));
    }
}

void MulticastSocket::sendMessage(std::span<const std::byte> message) {
    std::ranges::for_each(send_sockets_, [&](auto& socket) {
        socket.send_to(asio::const_buffer(message.data(), message.size()), multicast_endpoint_);
    });
}

std::optional<MulticastMessage> MulticastSocket::recvMessage(std::chrono::steady_clock::duration timeout) {
    MulticastMessage message {};
    message.content.resize(MESSAGE_BUFFER);
    asio::ip::udp::endpoint sender_endpoint {};

    // Receive as future
    auto length_future = recv_socket_.async_receive_from(asio::buffer(message.content), sender_endpoint, asio::use_future);

    // Run IO context for timeout
    io_context_.restart();
    io_context_.run_for(timeout);

    // If IO context not stopped, then no message received
    if(!io_context_.stopped()) {
        // Cancel async operations
        recv_socket_.cancel();
        return std::nullopt;
    }

    message.address = sender_endpoint.address().to_v4();
    message.content.resize(length_future.get());
    return message;
}
