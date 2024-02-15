/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <ctime>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>

#include "constellation/core/message/Header.hpp"
#include "constellation/core/utils/casts.hpp"

using namespace Catch::Matchers;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::string_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Basic Header Functions", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    const CSCP1Header cscp1_header {"senderCSCP", tp};

    REQUIRE_THAT(sv_to_string(cscp1_header.getSender()), Equals("senderCSCP"));
    REQUIRE(cscp1_header.getTime() == tp);
    REQUIRE(cscp1_header.getTags().empty());
    REQUIRE_THAT(cscp1_header.to_string(), ContainsSubstring("CSCP1"));
}

TEST_CASE("String Output", "[core][core::message]") {
    // Get fixed timepoint (unix epoch)
    auto tp = std::chrono::system_clock::from_time_t(std::time_t(0));

    CMDP1Header cmdp1_header {"senderCMDP", tp};

    cmdp1_header.setTag("test_b", true);
    cmdp1_header.setTag("test_i", 7);
    cmdp1_header.setTag("test_d", 1.5);
    cmdp1_header.setTag("test_s", "String"s);
    cmdp1_header.setTag("test_t", tp);

    const auto string_out = cmdp1_header.to_string();

    REQUIRE_THAT(string_out, ContainsSubstring("Header: CMDP1"));
    REQUIRE_THAT(string_out, ContainsSubstring("Sender: senderCMDP"));
    REQUIRE_THAT(string_out, ContainsSubstring("Time:   1970-01-01 00:00:00.000000000"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_b: true"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_i: 7"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_d: 1.5"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_s: String"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_t: 1970-01-01 00:00:00.000000000"));
}

TEST_CASE("Packing / Unpacking", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CDTP1Header cdtp1_header {"senderCDTP", tp};

    cdtp1_header.setTag("test_b", true);
    cdtp1_header.setTag("test_i", std::numeric_limits<std::int64_t>::max());
    cdtp1_header.setTag("test_d", std::numbers::pi);
    cdtp1_header.setTag("test_s", "String"s);
    cdtp1_header.setTag("test_t", tp);

    // Pack header
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, cdtp1_header);

    // Unpack header
    const auto cdtp1_header_unpacked = CDTP1Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()});

    // Compare unpacked header
    REQUIRE(cdtp1_header_unpacked.getTags().size() == 5);
    REQUIRE(std::get<bool>(cdtp1_header_unpacked.getTag("test_b")));
    REQUIRE(std::get<std::int64_t>(cdtp1_header_unpacked.getTag("test_i")) == std::numeric_limits<std::int64_t>::max());
    REQUIRE(std::get<double>(cdtp1_header_unpacked.getTag("test_d")) == std::numbers::pi);
    REQUIRE_THAT(std::get<std::string>(cdtp1_header_unpacked.getTag("test_s")), Equals("String"));
    REQUIRE(std::get<std::chrono::system_clock::time_point>(cdtp1_header_unpacked.getTag("test_t")) == tp);
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
