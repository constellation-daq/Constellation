/**
 * @file
 * @brief Helpers for ZeroMQ
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <charconv>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <zmq.hpp>

#include "constellation/build.hpp"

namespace constellation::utils {

    /**
     * @brief Port number for a network connection
     *
     * Note that most ports in Constellation are ephemeral ports, meaning that the port numbers are allocated dynamically.
     * See also https://en.wikipedia.org/wiki/Ephemeral_port.
     */
    using Port = std::uint16_t;

    /**
     * @brief Bind ZeroMQ socket to wildcard address with ephemeral port
     *
     * See also https://libzmq.readthedocs.io/en/latest/zmq_tcp.html.
     *
     * @param socket Reference to socket which should be bound
     * @return Ephemeral port assigned by operating system
     */
    CNSTLN_API inline Port bind_ephemeral_port(zmq::socket_t& socket) {
        Port port {};

        // Bind to wildcard address and port to let operating system assign an ephemeral port
        socket.bind("tcp://*:*");

        // Get address with ephemeral port via last endpoint
        const auto endpoint = socket.get(zmq::sockopt::last_endpoint);

        // Note: endpoint is always "tcp://0.0.0.0:XXXXX", thus port number starts at character 14
        const auto port_substr = std::string_view(endpoint).substr(14);
        std::from_chars(port_substr.cbegin(), port_substr.cend(), port);

        return port;
    }

    /**
     * @brief Return the global ZeroMQ context
     *
     * @note Since the global ZeroMQ context is static, static classes need to store an instance of the shared pointer.
     *
     * @return Shared pointer to the global ZeroMQ context
     */
    CNSTLN_API inline std::shared_ptr<zmq::context_t>& global_zmq_context() {
        static auto context = std::make_shared<zmq::context_t>();
        // Switch off blocky behavior of context - corresponds to setting linger = 0 for all sockets
        context->set(zmq::ctxopt::blocky, 0);
        return context;
    }

    CNSTLN_API inline std::set<std::string> get_broadcast_addresses() {
        std::set<std::string> addresses;

        // Obtain linked list of all local network interfaces
        struct ifaddrs* addrs = nullptr;
        struct ifaddrs* ifa = nullptr;
        if(getifaddrs(&addrs) != 0) {
            return {};
        }

        // Iterate through list of interfaces
        for(ifa = addrs; ifa != nullptr; ifa = ifa->ifa_next) {

            // Select only running interfaces and those providing IPV4:
            if(ifa->ifa_addr == nullptr || ((ifa->ifa_flags & IFF_RUNNING) == 0U) || ifa->ifa_addr->sa_family != AF_INET) {
                continue;
            }

            // Ensure that the interface holds a broadcast address
            if(((ifa->ifa_flags & IFF_BROADCAST) == 0U) ||
               ifa->ifa_ifu.ifu_broadaddr == nullptr) { // NOLINT(cppcoreguidelines-pro-type-union-access)
                continue;
            }

            char buffer[NI_MAXHOST] {
                0,
            }; // NOLINT(modernize-avoid-c-arrays)
            if(getnameinfo(ifa->ifa_ifu.ifu_broadaddr, // NOLINT(cppcoreguidelines-pro-type-union-access)
                           sizeof(struct sockaddr_in),
                           buffer,
                           NI_MAXHOST,
                           nullptr,
                           0,
                           NI_NUMERICHOST) == 0) {
                // Add to result list
                addresses.emplace(buffer);
            }
        }

        freeifaddrs(addrs);
        return addresses;
    }

} // namespace constellation::utils
