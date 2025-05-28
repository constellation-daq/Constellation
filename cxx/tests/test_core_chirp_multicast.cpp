/**
 * @file
 * @brief Tests for CHIRP multicast socket
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstddef>
#include <future>
#include <string> // IWYU pragma: keep
#include <vector>

#include <asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

#include "constellation/core/chirp/MulticastSocket.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/utils/casts.hpp"

#include "chirp_mock.hpp"

using namespace Catch::Matchers;
using namespace constellation::chirp;
using namespace constellation::networking;
using namespace constellation::protocol::CHIRP;
using namespace constellation::utils;
using namespace std::chrono_literals;
using namespace std::string_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Send and receive multicasts containing a string", "[chirp]") {
    MulticastSocket receiver {get_loopback_if(), asio::ip::address_v4(MULTICAST_ADDRESS), 49152};
    MulticastSocket sender {get_loopback_if(), asio::ip::address_v4(MULTICAST_ADDRESS), 49152};

    // Start receiving new message
    auto msg_future = std::async(&MulticastSocket::recvMessage, &receiver, 10ms);
    // Send message (string)
    auto msg_content = "test message"s;
    sender.sendMessage(to_byte_span(msg_content));
    // Receive message
    auto msg = msg_future.get();

    REQUIRE(msg.has_value());
    REQUIRE_THAT(msg.value().content, RangeEquals(to_byte_span(msg_content))); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("Send and receive multicasts containing binary content", "[chirp]") {
    MulticastSocket receiver {get_loopback_if(), asio::ip::address_v4(MULTICAST_ADDRESS), 49152};
    MulticastSocket sender {get_loopback_if(), asio::ip::address_v4(MULTICAST_ADDRESS), 49152};

    // Start receiving new message
    auto msg_future = std::async(&MulticastSocket::recvMessage, &receiver, 10ms);
    // Send message (bytes)
    auto msg_content = std::vector<std::byte>({std::byte('T'), std::byte('E'), std::byte('S'), std::byte('T')});
    sender.sendMessage(to_byte_span(msg_content));
    // Receive message
    auto msg = msg_future.get();

    REQUIRE(msg.has_value());
    REQUIRE_THAT(msg.value().content, RangeEquals(msg_content)); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("Get IP address of multicasts from localhost", "[chirp]") {
    MulticastSocket receiver {get_loopback_if(), asio::ip::address_v4(MULTICAST_ADDRESS), 49152};
    MulticastSocket sender {get_loopback_if(), asio::ip::address_v4(MULTICAST_ADDRESS), 49152};

    // Start receiving new message
    auto msg_future = std::async(&MulticastSocket::recvMessage, &receiver, 10ms);
    // Send message
    auto msg_content = "test message"s;
    sender.sendMessage(to_byte_span(msg_content));
    // Receive message
    auto msg = msg_future.get();

    REQUIRE(msg.has_value());
    REQUIRE(msg.value().address == asio::ip::make_address_v4("127.0.0.1")); // NOLINT(bugprone-unchecked-optional-access)
}

TEST_CASE("Send and receive multicasts asynchronously", "[chirp]") {
    MulticastSocket receiver {get_loopback_if(), asio::ip::address_v4(MULTICAST_ADDRESS), 49152};
    MulticastSocket sender {get_loopback_if(), asio::ip::address_v4(MULTICAST_ADDRESS), 49152};

    // Try receiving new message
    auto msg_opt_future = std::async(&MulticastSocket::recvMessage, &receiver, 10ms);
    // Send message
    auto msg_content = "test message"s;
    sender.sendMessage(to_byte_span(msg_content));
    // Receive message
    auto msg_opt = msg_opt_future.get();

    // Check that a message was received
    REQUIRE(msg_opt.has_value());

    // Get message
    const auto& msg = msg_opt.value(); // NOLINT(bugprone-unchecked-optional-access)

    REQUIRE_THAT(msg.content, RangeEquals(to_byte_span(msg_content)));
}

TEST_CASE("Get timeout on asynchronous multicast receive", "[chirp]") {
    MulticastSocket receiver {get_loopback_if(), asio::ip::address_v4(MULTICAST_ADDRESS), 49152};

    // Try receiving new message
    auto msg_opt = receiver.recvMessage(10ms);

    // No message send, thus check for timeout that there is no message
    REQUIRE_FALSE(msg_opt.has_value());
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
