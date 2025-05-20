/**
 * @file
 * @brief Implementation of the multicast sender
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "MulticastHandler.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <set>
#include <utility>
#include <vector>

#include <asio.hpp>

using namespace constellation::chirp;

constexpr std::size_t MESSAGE_BUFFER = 1024;
constexpr int MULTICAST_TTL = 8;

MulticastHandler::MulticastHandler(const std::set<asio::ip::address_v4>& interface_addresses,
                                   const asio::ip::address_v4& multicast_address,
                                   asio::ip::port_type multicast_port)
    : multicast_endpoint_(multicast_address, multicast_port) {

    // Create sockets
    sockets_.reserve(interface_addresses.size());
    for(const auto& interface_address : interface_addresses) {
        asio::ip::udp::socket socket {io_context_};

        // Open socket to set protocol
        socket.open(multicast_endpoint_.protocol());

        // Ensure socket can be bound by other programs
        socket.set_option(asio::ip::udp::socket::reuse_address(true));

        // Set Multicast TTL (aka network hops)
        socket.set_option(asio::ip::multicast::hops(MULTICAST_TTL));

        // Enable loopback interface
        socket.set_option(asio::ip::multicast::enable_loopback(true));

        // Set network interface
        socket.set_option(asio::ip::multicast::outbound_interface(interface_address));

        // Bind socket
        socket.bind(multicast_endpoint_);

        // Join multicast group
        socket.set_option(asio::ip::multicast::join_group(multicast_address, interface_address));

        // Store socket
        sockets_.emplace_back(std::move(socket));
    }
}

void MulticastHandler::sendMessage(std::span<const std::byte> message) {
    std::ranges::for_each(sockets_, [&](auto& socket) {
        socket.send_to(asio::const_buffer(message.data(), message.size()), multicast_endpoint_);
    });
}

std::vector<MulticastMessage> MulticastHandler::recvMessage(std::chrono::steady_clock::duration timeout) {
    // Allocate a buffer and endpoint for each socket
    std::vector<std::vector<std::byte>> buffers {};
    buffers.resize(sockets_.size());
    std::vector<asio::ip::udp::endpoint> sender_endpoints {};
    sender_endpoints.resize(sockets_.size());

    // Reserve a vector for the messages
    std::vector<MulticastMessage> messages {};
    messages.reserve(sockets_.size());

    for(std::size_t n = 0; n < sockets_.size(); ++n) {
        auto& socket = sockets_[n];
        auto& buffer = buffers[n];
        auto& sender_endpoint = sender_endpoints[n];

        // Allocate some memory for incoming messages
        buffer.resize(MESSAGE_BUFFER);

        // Register async receive function
        socket.async_receive_from(asio::buffer(buffer), sender_endpoint, [&](std::error_code ec, std::size_t size) {
            if(!ec && size > 0) {
                buffer.resize(size);
                messages.emplace_back(std::move(buffer), sender_endpoint.address().to_v4());
            }
        });
    }

    // Run IO context for the timeout period
    io_context_.restart();
    io_context_.run_for(timeout);

    // Cancel any lingering operations
    for(auto& socket : sockets_) {
        socket.cancel();
    }

    return messages;
}
