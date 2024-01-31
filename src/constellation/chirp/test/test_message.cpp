/**
 * @file
 * @brief Tests for CHIRP message formatting & content
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <cstdint>
#include <cstring>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/chirp/Message.hpp"

using namespace Catch::Matchers;
using namespace constellation::chirp;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("MD5 hashing with RFC 1321 reference implementation values", "[chirp][md5]") {
    // clang-format off
    REQUIRE_THAT(MD5Hash("").to_string(), Equals("d41d8cd98f00b204e9800998ecf8427e"));
    REQUIRE_THAT(MD5Hash("a").to_string(), Equals("0cc175b9c0f1b6a831c399e269772661"));
    REQUIRE_THAT(MD5Hash("abc").to_string(), Equals("900150983cd24fb0d6963f7d28e17f72"));
    REQUIRE_THAT(MD5Hash("message digest").to_string(), Equals("f96b697d7cb7938d525a2f31aaf161d0"));
    REQUIRE_THAT(MD5Hash("abcdefghijklmnopqrstuvwxyz").to_string(), Equals("c3fcd3d76192e4007dfb496cca67e13b"));
    REQUIRE_THAT(MD5Hash("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789").to_string(), Equals("d174ab98d277d9f5a5611c2c9f419d9f"));
    REQUIRE_THAT(MD5Hash("12345678901234567890123456789012345678901234567890123456789012345678901234567890").to_string(), Equals("57edf4a22be3c955ac49da2e2107b67a"));
    // clang-format on
}

TEST_CASE("Sorting of MD5 hashes", "[chirp][md5]") {
    REQUIRE_FALSE(MD5Hash("a") < MD5Hash("a"));
    REQUIRE(MD5Hash("a") < MD5Hash("b"));
}

TEST_CASE("Reconstruct CHIRP message from assembled blob", "[chirp][chirp::message]") {
    auto msg = Message(OFFER, "group", "host", CONTROL, 47890);
    auto asm_msg = msg.Assemble();
    auto msg_reconstructed = Message(asm_msg);

    REQUIRE(msg.GetType() == msg_reconstructed.GetType());
    REQUIRE(msg.GetGroupID() == msg_reconstructed.GetGroupID());
    REQUIRE(msg.GetHostID() == msg_reconstructed.GetHostID());
    REQUIRE(msg.GetServiceIdentifier() == msg_reconstructed.GetServiceIdentifier());
    REQUIRE(msg.GetPort() == msg_reconstructed.GetPort());
}

TEST_CASE("Detect invalid length in CHIRP message", "[chirp][chirp::message]") {
    std::vector<std::uint8_t> msg_data {};
    msg_data.resize(CHIRP_MESSAGE_LENGTH + 1);

    REQUIRE_THROWS_WITH(Message {msg_data},
                        Equals("Message length is not " + std::to_string(CHIRP_MESSAGE_LENGTH) + " bytes"));
}

TEST_CASE("Detect invalid identifier in CHIRP message", "[chirp][chirp::message]") {
    auto msg = Message(REQUEST, "group", "host", HEARTBEAT, 0);
    auto asm_msg = msg.Assemble();
    asm_msg[0] = 'X';

    REQUIRE_THROWS_WITH(Message {asm_msg}, Equals("Not a CHIRP broadcast"));
}

TEST_CASE("Detect invalid version in CHIRP message", "[chirp][chirp::message]") {
    auto msg = Message(REQUEST, "group", "host", HEARTBEAT, 0);
    auto asm_msg = msg.Assemble();
    asm_msg[5] = '2';

    REQUIRE_THROWS_WITH(Message {asm_msg}, Equals("Not a CHIRP v1 broadcast"));
}

TEST_CASE("Detect invalid message type in CHIRP message", "[chirp][chirp::message]") {
    auto msg = Message(static_cast<MessageType>(255), "group", "host", DATA, 0);
    auto asm_msg = msg.Assemble();

    REQUIRE_THROWS_WITH(Message {asm_msg}, Equals("Message Type invalid"));
}

TEST_CASE("Detect invalid service identifier in CHIRP message", "[chirp][chirp::message]") {
    auto msg = Message(OFFER, "group", "host", static_cast<ServiceIdentifier>(255), 12345);
    auto asm_msg = msg.Assemble();

    REQUIRE_THROWS_WITH(Message {asm_msg}, Equals("Service Identifier invalid"));
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)