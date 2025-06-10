/**
 * @file
 * @brief Asio helper functions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <set>
#include <string>
#include <string_view>

#include <asio/ip/address_v4.hpp>

#include "constellation/build.hpp"
#include "constellation/core/networking/Port.hpp"

namespace constellation::networking {

    /**
     * @brief Get all interface addresses
     *
     * @return Set with all interface addresses
     */
    CNSTLN_API std::set<asio::ip::address_v4> get_interface_addresses();

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
