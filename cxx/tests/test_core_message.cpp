/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CDTP2Message.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/protocol/Protocol.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/msgpack.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::string_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Basic Header Functions", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    const CSCP1Message::Header cscp1_header {"senderCSCP", tp};

    REQUIRE_THAT(to_string(cscp1_header.getSender()), Equals("senderCSCP"));
    REQUIRE(cscp1_header.getTime() == tp);
    REQUIRE(cscp1_header.getTags().empty());
    REQUIRE_THAT(cscp1_header.to_string(), ContainsSubstring("CSCP1"));
}

TEST_CASE("Header String Output", "[core][core::message]") {
    // Get fixed timepoint (unix epoch)
    const std::chrono::system_clock::time_point tp {};

    CMDP1Message::Header cmdp1_header {"senderCMDP", tp};

    cmdp1_header.setTag("test_b", true);
    cmdp1_header.setTag("test_i", 7);
    cmdp1_header.setTag("test_d", 1.5);
    cmdp1_header.setTag("test_s", "String"s);
    cmdp1_header.setTag("test_t", tp);

    const auto string_out = cmdp1_header.to_string();

    REQUIRE_THAT(string_out, ContainsSubstring("Header: CMDP1"));
    REQUIRE_THAT(string_out, ContainsSubstring("Sender: senderCMDP"));
    REQUIRE_THAT(string_out, ContainsSubstring("Time:   1970-01-01 00:00:00.000000"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_b: true"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_i: 7"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_d: 1.5"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_s: String"));
    REQUIRE_THAT(string_out, ContainsSubstring("test_t: 1970-01-01 00:00:00.000000"));
}

TEST_CASE("Header Packing / Unpacking", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CSCP1Message::Header cscp1_header {"senderCSCP", tp};

    cscp1_header.setTag("test_b", true);
    cscp1_header.setTag("test_i", std::numeric_limits<std::int64_t>::max());
    cscp1_header.setTag("test_d", std::numbers::pi);
    cscp1_header.setTag("test_s", "String"s);
    cscp1_header.setTag("test_t", tp);
    cscp1_header.setTag("Test_C", 0);

    // Pack header
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, cscp1_header);

    // Unpack header
    const auto cscp1_header_unpacked = CSCP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()});

    // Compare unpacked header
    REQUIRE(cscp1_header_unpacked.getTags().size() == 6);
    REQUIRE(cscp1_header_unpacked.getTag<bool>("test_b"));
    REQUIRE(cscp1_header_unpacked.getTag<std::int64_t>("test_i") == std::numeric_limits<std::int64_t>::max());
    REQUIRE(cscp1_header_unpacked.getTag<double>("test_d") == std::numbers::pi);
    REQUIRE_THAT(cscp1_header_unpacked.getTag<std::string>("test_s"), Equals("String"));
    REQUIRE(cscp1_header_unpacked.getTag<std::chrono::system_clock::time_point>("test_t") == tp);
    REQUIRE(cscp1_header_unpacked.hasTag("tEst_C"));
    REQUIRE(cscp1_header_unpacked.getTag<int>("teSt_c") == 0);
}

TEST_CASE("Header Packing / Unpacking (invalid protocol)", "[core][core::message]") {
    const CSCP1Message::Header cscp1_header {"senderCSCP"};

    // Pack header
    msgpack::sbuffer sbuf {};
    // first pack version
    msgpack::pack(sbuf, "INVALID");
    // then sender
    msgpack::pack(sbuf, "SenderCSCP");
    // then time
    msgpack::pack(sbuf, std::chrono::system_clock::now());
    // then tags
    msgpack::pack(sbuf, Dictionary());

    // Check for wrong protocol to be picked up
    REQUIRE_THROWS_MATCHES(CMDP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}),
                           InvalidProtocolError,
                           Message("Invalid protocol identifier `INVALID`"));
}

TEST_CASE("Header Packing / Unpacking (unexpected protocol)", "[core][core::message]") {
    const CSCP1Message::Header cscp1_header {"senderCSCP"};

    // Pack header
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, cscp1_header);

    // Check for wrong protocol to be picked up
    REQUIRE_THROWS_MATCHES(CMDP1Message::Header::disassemble({to_byte_ptr(sbuf.data()), sbuf.size()}),
                           UnexpectedProtocolError,
                           Message("Received protocol `CSCP1` does not match expected identifier `CMDP1`"));
}

TEST_CASE("Message Assembly / Disassembly (CMDP1)", "[core][core::message]") {
    // Log message with logger topic
    CMDP1LogMessage log_msg {Level::STATUS, "Logger_Topic", {"senderCMDP"}, "log message"};
    auto log_frames = log_msg.assemble();

    auto log_msg2_raw = CMDP1Message::disassemble(log_frames);
    REQUIRE(log_msg2_raw.isLogMessage());
    REQUIRE_THAT(to_string(log_msg2_raw.getMessageTopic()), Equals("LOG/STATUS/LOGGER_TOPIC"));

    const auto log_msg2 = CMDP1LogMessage(std::move(log_msg2_raw));
    REQUIRE_THAT(log_msg2.getHeader().to_string(), ContainsSubstring("Sender: senderCMDP"));
    REQUIRE(log_msg2.isLogMessage());
    REQUIRE(log_msg2.getLogLevel() == Level::STATUS);
    REQUIRE_THAT(to_string(log_msg2.getLogTopic()), Equals("LOGGER_TOPIC"));
    REQUIRE_THAT(to_string(log_msg2.getLogMessage()), Equals("log message"));

    // Log message without logger topic (default logger)
    CMDP1LogMessage dl_log_msg {Level::STATUS, "", {"senderCMDP"}, "log message"};
    auto dl_log_frames = dl_log_msg.assemble();

    auto dl_log_msg2 = CMDP1LogMessage::disassemble(dl_log_frames);
    REQUIRE_THAT(to_string(dl_log_msg2.getLogTopic()), Equals(""));
}

TEST_CASE("Message Assembly / Disassembly (CMDP1, invalid number of frames)", "[core][core::message]") {
    CMDP1LogMessage log_msg {Level::STATUS, "", {"senderCMDP"}, ""};
    auto log_frames = log_msg.assemble();

    // Add invalid fourth frame
    log_frames.addstr("should not be here");

    REQUIRE_THROWS_MATCHES(CMDP1Message::disassemble(log_frames),
                           MessageDecodingError,
                           Message("Error decoding CMDP1 message: Invalid number of message frames"));
}

TEST_CASE("Message Assembly / Disassembly (CMDP1, invalid topic)", "[core][core::message]") {
    CMDP1LogMessage log_msg {Level::STATUS, "", {"senderCMDP"}, ""};
    auto log_frames = log_msg.assemble();

    // Add invalid fourth frame
    zmq::message_t invalid_topic {"INVALID/TOPIC"s};
    log_frames.at(0).swap(invalid_topic);

    REQUIRE_THROWS_MATCHES(
        CMDP1Message::disassemble(log_frames),
        MessageDecodingError,
        Message("Error decoding CMDP1 message: Invalid message topic `INVALID/TOPIC`, neither log nor telemetry message"));
}

TEST_CASE("Message Assembly / Disassembly (CMDP1, invalid log level)", "[core][core::message]") {
    CMDP1LogMessage log_msg {Level::STATUS, "", {"senderCMDP"}, ""};
    auto log_frames = log_msg.assemble();

    // Add invalid fourth frame
    zmq::message_t invalid_topic {"LOG/ERROR"s};
    log_frames.at(0).swap(invalid_topic);

    REQUIRE_THROWS_MATCHES(CMDP1Message::disassemble(log_frames),
                           MessageDecodingError,
                           Message("Error decoding CMDP1 message: `ERROR` is not a valid log level"));
}

TEST_CASE("Message Assembly / Disassembly (CSCP1)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CSCP1Message cscp1_msg {{"senderCSCP", tp}, {CSCP1Message::Type::SUCCESS, ""}};
    auto frames = cscp1_msg.assemble();

    auto cscp1_msg2 = CSCP1Message::disassemble(frames);

    REQUIRE_THAT(cscp1_msg2.getHeader().to_string(), ContainsSubstring("Sender: senderCSCP"));
    REQUIRE(cscp1_msg2.getVerb().first == CSCP1Message::Type::SUCCESS);
}

TEST_CASE("Incorrect message type (CMDP1)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    // Log message with logger topic
    CMDP1LogMessage log_msg {Level::STATUS, "logger", {"senderCMDP", tp}, ""};
    auto log_frames = log_msg.assemble();

    // Actually a stat message TODO(stephan.lachnit): use CMDPStatMessage once implemented
    zmq::message_t stat_topic {"STAT/STATI_TOPIC"s};
    log_frames.at(0).swap(stat_topic);
    REQUIRE_THROWS_MATCHES(CMDP1LogMessage::disassemble(log_frames),
                           IncorrectMessageType,
                           Message("Message type is incorrect: Not a log message"));
}

TEST_CASE("Message Payload (CSCP1)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CSCP1Message cscp1_msg {{"senderCSCP", tp}, {CSCP1Message::Type::SUCCESS, ""}};
    REQUIRE(cscp1_msg.getPayload().empty());

    // Add payload frame
    msgpack::sbuffer sbuf_header {};
    msgpack::pack(sbuf_header, "this is fine");
    cscp1_msg.addPayload(std::move(sbuf_header));

    // Assemble and disassemble message
    auto frames = cscp1_msg.assemble();
    auto cscp1_msg2 = CSCP1Message::disassemble(frames);

    // Retrieve payload
    const auto& data = cscp1_msg2.getPayload();
    REQUIRE_THAT(msgpack_unpack_to<std::string>(to_char_ptr(data.span().data()), data.span().size()),
                 Equals("this is fine"));
}

TEST_CASE("Message Payload (CSCP1, too many frames)", "[core][core::message]") {
    auto tp = std::chrono::system_clock::now();

    CSCP1Message cscp1_message {{"senderCSCP", tp}, {CSCP1Message::Type::SUCCESS, ""}};
    auto frames = cscp1_message.assemble();

    // Attach additional frames:
    msgpack::sbuffer sbuf_header {};
    msgpack::pack(sbuf_header, "this is fine");
    auto payload = PayloadBuffer(std::move(sbuf_header));
    frames.add(payload.to_zmq_msg_copy());
    frames.add(payload.to_zmq_msg_release());

    // Check for excess frame detection
    REQUIRE_THROWS_MATCHES(CSCP1Message::disassemble(frames),
                           MessageDecodingError,
                           Message("Error decoding CSCP1 message: Incorrect number of message frames"));
}

// CDTP2

TEST_CASE("CDTP2 DATA Packing / Unpacking", "[core][core::message]") {
    // Create some dummy data
    const std::vector<std::int32_t> vec_1 {1, 2, 3, 4};
    const std::vector<std::int32_t> vec_2 {5, 6, 7, 8, 9};
    const std::vector<std::int32_t> vec_3 {3, 1, 4, 1, 5, 9};
    // Create DATA message
    auto data_message = CDTP2Message("sender", CDTP2Message::Type::DATA, 2);
    auto data_record_1 = CDTP2Message::DataRecord(1, {{{"block", 1}}});
    data_record_1.addBlock({std::vector(vec_1)});
    REQUIRE(data_record_1.countPayloadBytes() == 16);
    data_message.addDataRecord(std::move(data_record_1));
    auto data_record_2 = CDTP2Message::DataRecord(2, {{{"block", 2}}}, 2);
    data_record_2.addTag("vecs", "2&3");
    data_record_2.addBlock({std::vector(vec_2)});
    data_record_2.addBlock({std::vector(vec_3)});
    REQUIRE(data_record_2.countPayloadBytes() == 44);
    data_message.addDataRecord(std::move(data_record_2));
    REQUIRE(data_message.countPayloadBytes() == 60);
    auto zmq_mpm = data_message.assemble();
    // Decode DATA message
    const auto data_message_decoded = CDTP2Message::disassemble(zmq_mpm);
    REQUIRE_THAT(std::string(data_message_decoded.getSender()), Equals("sender"));
    REQUIRE(data_message_decoded.getType() == CDTP2Message::Type::DATA);
    REQUIRE(data_message_decoded.getDataRecords().size() == 2);
    const auto& data_record_1_decoded = data_message_decoded.getDataRecords().at(0);
    REQUIRE(data_record_1_decoded.getSequenceNumber() == 1);
    REQUIRE(data_record_1_decoded.getTags().at("block") == 1);
    REQUIRE(data_record_1_decoded.countPayloadBytes() == 16);
    REQUIRE(data_record_1_decoded.countBlocks() == 1);
    REQUIRE_THAT(data_record_1_decoded.getBlocks().at(0).span(), RangeEquals(PayloadBuffer(std::vector(vec_1)).span()));
    const auto& data_record_2_decoded = data_message_decoded.getDataRecords().at(1);
    REQUIRE(data_record_2_decoded.getSequenceNumber() == 2);
    REQUIRE(data_record_2_decoded.getTags().at("block") == 2);
    REQUIRE(data_record_2_decoded.getTags().at("vecs") == "2&3"s);
    REQUIRE(data_record_2_decoded.countPayloadBytes() == 44);
    REQUIRE(data_record_2_decoded.countBlocks() == 2);
    REQUIRE_THAT(data_record_2_decoded.getBlocks().at(0).span(), RangeEquals(PayloadBuffer(std::vector(vec_2)).span()));
    REQUIRE_THAT(data_record_2_decoded.getBlocks().at(1).span(), RangeEquals(PayloadBuffer(std::vector(vec_3)).span()));
}

TEST_CASE("CDTP2 BOR Packing / Unpacking", "[core][core::message]") {
    // Create dummy user tags and configuration
    Dictionary user_tags {};
    user_tags["test"] = 1234;
    Configuration config {};
    config.set("used", "this is used", true);
    config.set("unused", "this is not used", false);
    // Create BOR message
    const auto bor_message = CDTP2BORMessage("sender", user_tags, config);
    auto zmq_mpm = bor_message.assemble();
    // Decode BOR message and check content
    const auto bor_message_decoded = CDTP2BORMessage(CDTP2Message::disassemble(zmq_mpm));
    REQUIRE(bor_message_decoded.getUserTags().at("test") == 1234);
    const auto config_decoded = bor_message_decoded.getConfiguration();
    REQUIRE(config_decoded.get<std::string>("used") == "this is used");
    REQUIRE_FALSE(config_decoded.has("unused"));
}

TEST_CASE("CDTP2 EOR Packing / Unpacking", "[core][core::message]") {
    // Create dummy user tags and run metadata
    Dictionary user_tags {};
    user_tags["test"] = 1234;
    Dictionary run_metadata {};
    run_metadata["run_finished"] = "yes";
    // Create EOR message
    const auto eor_message = CDTP2EORMessage("sender", user_tags, run_metadata);
    auto zmq_mpm = eor_message.assemble();
    // Decode EOR message and check content
    const auto eor_message_decoded = CDTP2EORMessage(CDTP2Message::disassemble(zmq_mpm));
    REQUIRE(eor_message_decoded.getUserTags().at("test") == 1234);
    REQUIRE(eor_message_decoded.getRunMetadata().at("run_finished") == "yes"s);
}

TEST_CASE("CDTP2 Invalid Number of Frames", "[core][core::message]") {
    zmq::multipart_t zmq_mpm {};
    zmq_mpm.addstr("msg1");
    zmq_mpm.addstr("msg2");
    REQUIRE_THROWS_MATCHES(
        CDTP2Message::disassemble(zmq_mpm),
        MessageDecodingError,
        Message("Error decoding CDTP2 message: Wrong number of ZeroMQ frames, exactly one frame expected"));
}

TEST_CASE("CDTP2 Unexpected Protocol", "[core][core::message]") {
    msgpack::sbuffer sbuf {};
    msgpack_pack(sbuf, get_protocol_identifier(Protocol::CDTP1));
    zmq::multipart_t zmq_mpm {};
    zmq_mpm.addmem(sbuf.data(), sbuf.size());
    REQUIRE_THROWS_MATCHES(CDTP2Message::disassemble(zmq_mpm),
                           UnexpectedProtocolError,
                           Message("Received protocol `CDTP1` does not match expected identifier `CDTP2`"));
}

TEST_CASE("CDTP2 Invalid Protocol", "[core][core::message]") {
    msgpack::sbuffer sbuf {};
    msgpack_pack(sbuf, "INVALID");
    zmq::multipart_t zmq_mpm {};
    zmq_mpm.addmem(sbuf.data(), sbuf.size());
    REQUIRE_THROWS_MATCHES(
        CDTP2Message::disassemble(zmq_mpm), InvalidProtocolError, Message("Invalid protocol identifier `INVALID`"));
}

TEST_CASE("CDTP2 Incorrect Message Type", "[core][core::message]") {
    auto data_message_1 = CDTP2Message("sender", CDTP2Message::Type::DATA, 0);
    REQUIRE_THROWS_MATCHES(CDTP2BORMessage(std::move(data_message_1)),
                           IncorrectMessageType,
                           Message("Message type is incorrect: Not a BOR message"));
    auto data_message_2 = CDTP2Message("sender", CDTP2Message::Type::DATA, 0);
    REQUIRE_THROWS_MATCHES(CDTP2EORMessage(std::move(data_message_2)),
                           IncorrectMessageType,
                           Message("Message type is incorrect: Not an EOR message"));
}

TEST_CASE("CDTP2 Invalid Number of Data Records", "[core][core::message]") {
    auto data_message_1 = CDTP2Message("sender", CDTP2Message::Type::BOR, 0);
    REQUIRE_THROWS_MATCHES(
        CDTP2BORMessage(std::move(data_message_1)),
        MessageDecodingError,
        Message("Error decoding CDTP2 BOR message: Wrong number of data records, exactly two data records expected"));
    auto data_message_2 = CDTP2Message("sender", CDTP2Message::Type::EOR, 0);
    REQUIRE_THROWS_MATCHES(
        CDTP2EORMessage(std::move(data_message_2)),
        MessageDecodingError,
        Message("Error decoding CDTP2 EOR message: Wrong number of data records, exactly two data records expected"));
}

TEST_CASE("CDTP2 Invalid Data Records", "[core][core::message]") {
    msgpack::sbuffer sbuf {};
    msgpack_pack(sbuf, get_protocol_identifier(Protocol::CDTP2));
    msgpack_pack(sbuf, "sender");
    msgpack_pack(sbuf, std::to_underlying(CDTP2Message::Type::DATA));
    msgpack_pack(sbuf, "not an array");
    zmq::multipart_t zmq_mpm {};
    zmq_mpm.addmem(sbuf.data(), sbuf.size());
    REQUIRE_THROWS_MATCHES(CDTP2Message::disassemble(zmq_mpm),
                           MessageDecodingError,
                           Message("Error decoding CDTP2 message: Error unpacking data: data records are not in an array"));
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
