/**
 * @file
 * @brief Asio helper functions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <asio/ip/address_v4.hpp>

#include "constellation/build.hpp"
#include "constellation/core/networking/Port.hpp"

namespace constellation::networking {

    /**
     * @brief Interface containing its name and address
     */
    struct Interface {
        /** Interface name */
        std::string name;

        /** Interface address */
        asio::ip::address_v4 address;
    };

    /**
     * @brief Get hostname
     *
     * @note This function sanitized the hostname by replacing hyphens and dots with underscores
     */
    CNSTLN_API std::string get_hostname();

    /**
     * @brief Get all interfaces
     *
     * @return List with all interfaces
     */
    CNSTLN_API std::vector<Interface> get_interfaces();

    /**
     * @brief Get interfaces matching a list of interface names
     *
     * @return List of interfaces with matching names
     */
    CNSTLN_API std::vector<Interface> get_interfaces(const std::vector<std::string>& interface_names);

    /**
     * @brief Build a URI from an IP address and a port
     *
     * @param address IPv4 address
     * @param port Port
     * @param protocol Protocol (without `://`), can be empty
     * @return URI in the form `protocol://address:port`
     */
    CNSTLN_API std::string to_uri(const asio::ip::address_v4& address, Port port, std::string_view protocol = "tcp");

} // namespace constellation::networking
