/**
 * @file
 * @brief Multicast socket
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <asio/ip/address_v4.hpp>
#include <asio/ip/udp.hpp>

#include "constellation/build.hpp"
#include "constellation/core/networking/asio_helpers.hpp"

namespace constellation::chirp {

    /** Incoming multicast message */
    struct MulticastMessage {
        /** Content of the message in bytes */
        std::vector<std::byte> content;

        /** Address from which the message was received */
        asio::ip::address_v4 address;
    };

    /** Multicast handler for multicast message */
    class MulticastSocket {
    public:
        /**
         * Construct multicast handler
         *
         * @param interfaces List of interfaces for outgoing messages
         * @param multicast_address Multicast address
         * @param multicast_port Multicast port
         */
        CNSTLN_API MulticastSocket(const std::vector<networking::Interface>& interfaces,
                                   const asio::ip::address_v4& multicast_address,
                                   asio::ip::port_type multicast_port);

        /**
         * Send multicast message to all interfaces
         *
         * @param message Message in bytes
         */
        CNSTLN_API void sendMessage(std::span<const std::byte> message);

        /**
         * Receive multicast message within a timeout
         *
         * @param timeout Duration for which to block function call
         * @return Multicast message if received
         */
        CNSTLN_API std::optional<MulticastMessage> recvMessage(std::chrono::steady_clock::duration timeout);

    private:
        asio::io_context io_context_;
        asio::ip::udp::socket recv_socket_;
        std::vector<asio::ip::udp::socket> send_sockets_;
        asio::ip::udp::endpoint multicast_endpoint_;
    };

} // namespace constellation::chirp
