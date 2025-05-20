/**
 * @file
 * @brief Multicast sender
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <set>
#include <span>
#include <vector>

#include <asio/ip/address_v4.hpp>
#include <asio/ip/udp.hpp>

#include "constellation/build.hpp"

namespace constellation::chirp {

    /** Incoming multicast message */
    struct MulticastMessage {
        /** Content of the message in bytes */
        std::vector<std::byte> content;

        /** Address from which the message was received */
        asio::ip::address_v4 address;
    };

    /** Multicast handler for multicast message */
    class MulticastHandler {
    public:
        /**
         * Construct multicast handler
         *
         * @param interface_addresses Set of interface addresses for outgoing messages
         * @param multicast_address Multicast address
         * @param multicast_port Multicast port
         */
        CNSTLN_API MulticastHandler(const std::set<asio::ip::address_v4>& interface_addresses,
                                    const asio::ip::address_v4& multicast_address,
                                    asio::ip::port_type multicast_port);

        /**
         * Send multicast message to all interfaces
         *
         * @param message Message in bytes
         */
        CNSTLN_API void sendMessage(std::span<const std::byte> message);

        /**
         * Send multicast message to one interface
         *
         * @param message Message in bytes
         * @param interface_address Address of interface for which to send message
         */
        CNSTLN_API void sendMessage(std::span<const std::byte> message, const asio::ip::address_v4& interface_address);

        /**
         * Receive multicast messages within a timeout
         *
         * @param timeout Duration for which to block function call
         * @return Vector with received messages (might be empty)
         */
        CNSTLN_API std::vector<MulticastMessage> recvMessage(std::chrono::steady_clock::duration timeout);

    private:
        asio::io_context io_context_;
        asio::ip::udp::endpoint multicast_endpoint_;
        std::vector<asio::ip::udp::socket> sockets_;
    };
} // namespace constellation::chirp
