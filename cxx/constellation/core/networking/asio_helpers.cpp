/**
 * @file
 * @brief Implementation of Asio helper functions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "asio_helpers.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include <asio.hpp>
#include <asio/ip/address_v4.hpp>
#ifndef _WIN32
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "constellation/core/networking/exceptions.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::networking;
using namespace constellation::utils;

std::vector<Interface> constellation::networking::get_interfaces() {
    std::vector<Interface> interfaces {};

#if defined(_WIN32)

    // TODO(stephan.lachnit): implement this on Windows, right now take default address
    addresses.emplace(asio::ip::address_v4::any());

#else

    // Obtain linked list of all local network interfaces
    struct ifaddrs* addrs = nullptr;
    struct ifaddrs* ifa = nullptr;
    if(getifaddrs(&addrs) != 0) {
        throw NetworkError("Unable to get list of interfaces");
    }

    // Iterate through list of interfaces
    for(ifa = addrs; ifa != nullptr; ifa = ifa->ifa_next) {

        // Select only running interfaces and those providing IPV4
        if(ifa->ifa_addr == nullptr || ifa->ifa_name == nullptr || (ifa->ifa_flags & IFF_RUNNING) == 0U ||
           ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        // Ensure that the interface is multicast capable (except for loopback)
        if((ifa->ifa_flags & IFF_MULTICAST) == 0U && (ifa->ifa_flags & IFF_LOOPBACK) == 0U) {
            continue;
        }

        char buffer[NI_MAXHOST];      // NOLINT(modernize-avoid-c-arrays)
        if(getnameinfo(ifa->ifa_addr, // NOLINT(cppcoreguidelines-pro-type-union-access)
                       sizeof(struct sockaddr_in),
                       buffer, // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                       sizeof(buffer),
                       nullptr,
                       0,
                       NI_NUMERICHOST) == 0) {
            try {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                interfaces.emplace_back(ifa->ifa_name, asio::ip::make_address_v4(buffer));
            } catch(const asio::system_error& error) {
                continue;
            }
        }
    }

    freeifaddrs(addrs);

#endif

    return interfaces;
}

std::vector<Interface> constellation::networking::get_interfaces(std::vector<std::string> interface_names) {
    std::vector<Interface> interfaces {};
    const auto all_interfaces = get_interfaces();

    std::ranges::for_each(interface_names, [&](const auto& interface_name) {
        const auto interface_it =
            std::ranges::find(all_interfaces, interface_name, [](const auto& interface) { return interface.name; });
        if(interface_it == all_interfaces.end()) {
            throw NetworkError("Interface `" + interface_name + "` does not exist or is not suitable");
        }
        interfaces.emplace_back(*interface_it);
    });

    return interfaces;
}

std::string constellation::networking::to_uri(const asio::ip::address_v4& address, Port port, std::string_view protocol) {
    std::string uri {};
    if(!protocol.empty()) {
        uri += protocol;
        uri += "://";
    }
    uri += range_to_string(address.to_bytes(), ".");
    uri += ":";
    uri += to_string(port);
    return uri;
}
