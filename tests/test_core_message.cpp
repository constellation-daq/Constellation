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

#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/CMDP1Header.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/utils/casts.hpp"

using namespace Catch::Matchers;
using namespace constellation::message;
using namespace constellation::utils;
using namespace std::literals::string_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Basic Header Functions", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    const CSCP1Message::Header cscp1_header {"senderCSCP", tp};

    REQUIRE_THAT(to_string(cscp1_header.getSender()), Equals("senderCSCP"));
    REQUIRE(cscp1_header.getTime() == tp);
    REQUIRE(cscp1_header.getTags().empty());
    REQUIRE_THAT(cscp1_header.to_string(), ContainsSubstring("CSCP1"));

    const CDTP1Message::Header cdtp1_header {"senderCDTP", 0, CDTP1Message::Type::BOR, tp};

    REQUIRE_THAT(to_string(cdtp1_header.getSender()), Equals("senderCDTP"));
    REQUIRE(cdtp1_header.getType() == CDTP1Message::Type::BOR);
    REQUIRE(cdtp1_header.getTime() == tp);
    REQUIRE(cdtp1_header.getTags().empty());
    REQUIRE_THAT(cdtp1_header.to_string(), ContainsSubstring("CDTP1"));
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

TEST_CASE("String Output (CDTP1)", "[core][core::message]") {
    const CDTP1Message::Header cdtp1_header {"senderCMDP", 1234, CDTP1Message::Type::DATA};

    const auto string_out = cdtp1_header.to_string();

    REQUIRE_THAT(string_out, ContainsSubstring("Type:   DATA"));
    REQUIRE_THAT(string_out, ContainsSubstring("Seq No: 1234"));
}

TEST_CASE("Packing / Unpacking", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CSCP1Message::Header cscp1_header {"senderCSCP", tp};

    cscp1_header.setTag("test_b", true);
    cscp1_header.setTag("test_i", std::numeric_limits<std::int64_t>::max());
    cscp1_header.setTag("test_d", std::numbers::pi);
    cscp1_header.setTag("test_s", "String"s);
    cscp1_header.setTag("test_t", tp);

    // Pack header
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, cscp1_header);

    // Unpack header
    const auto cscp1_header_unpacked = CSCP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()});

    // Compare unpacked header
    REQUIRE(cscp1_header_unpacked.getTags().size() == 5);
    REQUIRE(std::get<bool>(cscp1_header_unpacked.getTag("test_b")));
    REQUIRE(std::get<std::int64_t>(cscp1_header_unpacked.getTag("test_i")) == std::numeric_limits<std::int64_t>::max());
    REQUIRE(std::get<double>(cscp1_header_unpacked.getTag("test_d")) == std::numbers::pi);
    REQUIRE_THAT(std::get<std::string>(cscp1_header_unpacked.getTag("test_s")), Equals("String"));
    REQUIRE(std::get<std::chrono::system_clock::time_point>(cscp1_header_unpacked.getTag("test_t")) == tp);
}

TEST_CASE("Packing / Unpacking (unexpected protocol)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    const CSCP1Message::Header cscp1_header {"senderCSCP", tp};

    // Pack header
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, cscp1_header);

    // Check for wrong protocol to be picked up
    REQUIRE_THROWS_AS(CMDP1Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}), UnexpectedProtocolError);
}

TEST_CASE("Packing / Unpacking (too many frames)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CSCP1Message cscp1_message {{"senderCSCP", tp}, {CSCP1Message::Type::SUCCESS, ""}};
    auto frames = cscp1_message.assemble();

    // Attach additional frames:
    msgpack::sbuffer sbuf_header {};
    msgpack::pack(sbuf_header, "this is fine");
    frames.addmem(sbuf_header.data(), sbuf_header.size());
    frames.addmem(sbuf_header.data(), sbuf_header.size());

    // Check for excess frame detection
    REQUIRE_THROWS_AS(CSCP1Message::disassemble(frames), MessageDecodingError);
}

TEST_CASE("Packing / Unpacking (CDTP1)", "[core][core::message]") {
    constexpr std::uint64_t seq_no = 1234;
    const CDTP1Message::Header cdtp1_header {"senderCDTP", seq_no, CDTP1Message::Type::EOR};

    // Pack header
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, cdtp1_header);

    // Unpack header
    const auto cdtp1_header_unpacked = CDTP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()});

    // Compare unpacked header
    REQUIRE(cdtp1_header_unpacked.getType() == CDTP1Message::Type::EOR);
    REQUIRE(cdtp1_header_unpacked.getSequenceNumber() == seq_no);
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
