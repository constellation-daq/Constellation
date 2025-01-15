/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <map>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/listener/LogListener.hpp"

#include "chirp_mock.hpp"
#include "cmdp_mock.hpp"

using namespace Catch::Matchers;
using namespace constellation;
using namespace constellation::listener;
using namespace constellation::log;
using namespace constellation::utils;

TEST_CASE("Global log level", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Set global log subscription level
    listener.setGlobalLogLevel(Level::INFO);

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort());

    // Pop subscription messages (note: subscriptions come alphabetically)
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/CRITICAL"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/INFO"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/WARNING"));

    // Check global subscription is not returned in topic subscriptions
    REQUIRE_THAT(listener.getLogTopicSubscriptions(), RangeEquals(std::map<std::string, Level>({})));
    REQUIRE(listener.getGlobalLogLevel() == Level::INFO);

    // Reduce global level (note: subscriptions come alphabetically)
    listener.setGlobalLogLevel(Level::TRACE);
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/DEBUG"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/TRACE"));

    // Increase global level (note: unsubscriptions come alphabetically)
    listener.setGlobalLogLevel(Level::STATUS);
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/DEBUG"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/INFO"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/TRACE"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/WARNING"));

    // Turn off global subscription (note: unsubscriptions come alphabetically)
    listener.setGlobalLogLevel(Level::OFF);
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/CRITICAL"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/STATUS"));
}

TEST_CASE("Topic subscriptions", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Subscribe to topic
    listener.subscribeLogTopic("FSM", Level::INFO);

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort());

    // Pop subscription messages (note: subscriptions come alphabetically)
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/CRITICAL/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/INFO/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/WARNING/FSM"));

    // Subscribe to new topic (note: subscriptions come alphabetically)
    listener.subscribeLogTopic("SATELLITE", Level::WARNING);
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/CRITICAL/SATELLITE"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS/SATELLITE"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/WARNING/SATELLITE"));

    // Check subscribed topics
    REQUIRE_THAT(listener.getLogTopicSubscriptions(),
                 RangeEquals(std::map<std::string, Level>({{"FSM", Level::INFO}, {"SATELLITE", Level::WARNING}})));

    // Unsubscribe from a topic (note: unsubscriptions come alphabetically)
    listener.unsubscribeLogTopic("SATELLITE");
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/CRITICAL/SATELLITE"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/STATUS/SATELLITE"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/WARNING/SATELLITE"));

    // Check subscribed topics again
    REQUIRE_THAT(listener.getLogTopicSubscriptions(), RangeEquals(std::map<std::string, Level>({{"FSM", Level::INFO}})));
}

TEST_CASE("Extra topic subscriptions", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort());

    // Subscribe to extra topic
    listener.subscribeExtaLogTopic(to_string(sender.getName()), "FSM", Level::INFO);

    // Check subscription messages (note: subscriptions come alphabetically)
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/CRITICAL/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/INFO/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/WARNING/FSM"));

    // Check extra log topic subscriptions
    REQUIRE_THAT(listener.getExtraLogTopicSubscriptions(to_string(sender.getName())),
                 RangeEquals(std::map<std::string, Level>({{"FSM", Level::INFO}})));

    // Unsubscribe from extra topic (note: unsubscriptions come alphabetically)
    listener.unsubscribeExtraLogTopic(to_string(sender.getName()), "FSM");
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/CRITICAL/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/INFO/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/STATUS/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/WARNING/FSM"));
}

TEST_CASE("No empty topic subscription", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort());

    // Subscribe to empty topic
    listener.subscribeLogTopic("", Level::DEBUG);

    // Check that no subscription message is received
    REQUIRE_FALSE(sender.canRecv());

    // Check that subscription is not stored
    REQUIRE_THAT(listener.getLogTopicSubscriptions(), RangeEquals(std::map<std::string, Level>({})));

    listener.unsubscribeLogTopic("");
}

TEST_CASE("Empty extra topic subscription", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    auto chirp_manager = create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), chirp::MONITORING, sender.getPort());

    // Set global log topic
    listener.setGlobalLogLevel(Level::INFO);

    // Check subscription messages (note: subscriptions come alphabetically)
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/CRITICAL"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/INFO"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/WARNING"));

    // Subscribe to empty topic for host (note: subscriptions come alphabetically)
    listener.subscribeExtaLogTopic(to_string(sender.getName()), "", Level::TRACE);
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/DEBUG"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/TRACE"));

    // Increase empty topic for host (note: unsubscriptions come alphabetically)
    listener.subscribeExtaLogTopic(to_string(sender.getName()), "", Level::WARNING);
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/DEBUG"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/TRACE"));

    // Increase global log level
    listener.setGlobalLogLevel(Level::STATUS);

    // Check extra log topic subscriptions
    REQUIRE_THAT(listener.getExtraLogTopicSubscriptions(to_string(sender.getName())),
                 RangeEquals(std::map<std::string, Level>({{"", Level::WARNING}})));
}
