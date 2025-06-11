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

#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
// clang-format on
#else
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "constellation/core/networking/exceptions.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/utils/string.hpp"
#ifdef _WIN32
#include "constellation/core/utils/windows.hpp"
#endif

using namespace constellation::networking;
using namespace constellation::utils;

std::string constellation::networking::get_hostname() {
    auto host_name = asio::ip::host_name();
    std::ranges::replace(host_name, '-', '_');
    std::ranges::replace(host_name, '.', '_');
    return host_name;
}

std::vector<Interface> constellation::networking::get_interfaces() {
    std::vector<Interface> interfaces {};

#if defined(_WIN32)

    // Allocate a 15 KiB buffer for adapter info
    ULONG buffer_size = 15 * 1024;
    std::vector<char> buffer {};
    buffer.resize(buffer_size);
    IP_ADAPTER_ADDRESSES* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    // Only return IPv4 interfaces
    ULONG family = AF_INET;
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_PREFIX;

    // Get adapters
    DWORD result = GetAdaptersAddresses(family, flags, nullptr, adapters, &buffer_size);

    // Try again if buffer was too small
    if(result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(buffer_size);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        result = GetAdaptersAddresses(family, flags, nullptr, adapters, &buffer_size);
    }

    if(result != NO_ERROR) {
        throw NetworkError("Unable to get list of interfaces");
    }

    for(IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        // Select only running interfaces
        if(adapter->OperStatus != IfOperStatusUp) {
            continue;
        }

        // Iterate through addresses of the current adapter
        for(IP_ADAPTER_UNICAST_ADDRESS* ua = adapter->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
            if(ua->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }

            sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            char buffer[INET_ADDRSTRLEN];
            if(inet_ntop(AF_INET, &(ipv4->sin_addr), buffer, INET_ADDRSTRLEN)) {
                try {
                    interfaces.emplace_back(to_std_string(adapter->FriendlyName), asio::ip::make_address_v4(buffer));
                } catch(const asio::system_error&) {
                    continue;
                }
            }
        }
    }

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
            std::ranges::find(all_interfaces, interface_name, [](const auto& if_s) { return if_s.name; });
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
