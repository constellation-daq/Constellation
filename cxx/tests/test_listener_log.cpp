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

#include "constellation/core/log/Level.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/listener/LogListener.hpp"

#include "chirp_mock.hpp"
#include "cmdp_mock.hpp"

using namespace Catch::Matchers;
using namespace constellation::listener;
using namespace constellation::log;
using namespace constellation::protocol;
using namespace constellation::utils;

TEST_CASE("Global log level", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Set global log subscription level
    listener.setGlobalLogLevel(Level::INFO);

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    sender.mockChirpService();

    // Pop subscription messages (note: subscriptions come alphabetically if iterated from set)
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/CRITICAL"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/INFO"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/WARNING"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG?"));

    // Check global subscription is not returned in topic subscriptions
    REQUIRE_THAT(listener.getLogTopicSubscriptions(), RangeEquals(std::map<std::string, Level>({})));
    REQUIRE(listener.getGlobalLogLevel() == Level::INFO);

    // Reduce global level
    listener.setGlobalLogLevel(Level::TRACE);
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/TRACE"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/DEBUG"));

    // Increase global level
    listener.setGlobalLogLevel(Level::STATUS);
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/TRACE"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/DEBUG"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/INFO"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/WARNING"));

    // Turn off global subscription
    listener.setGlobalLogLevel(Level::OFF);
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/STATUS"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/CRITICAL"));

    listener.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Topic subscriptions", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Subscribe to topic
    listener.subscribeLogTopic("FSM", Level::INFO);

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    sender.mockChirpService();

    // Pop subscription messages (note: subscriptions come alphabetically if iterated from set)
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/CRITICAL/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/INFO/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/WARNING/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG?"));

    // Subscribe to new topic
    listener.subscribeLogTopic("CTRL", Level::WARNING);
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/WARNING/CTRL"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS/CTRL"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/CRITICAL/CTRL"));

    // Check subscribed topics
    REQUIRE_THAT(listener.getLogTopicSubscriptions(),
                 RangeEquals(std::map<std::string, Level>({{"FSM", Level::INFO}, {"CTRL", Level::WARNING}})));

    // Unsubscribe from a topic
    listener.unsubscribeLogTopic("CTRL");
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/WARNING/CTRL"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/STATUS/CTRL"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/CRITICAL/CTRL"));

    // Check subscribed topics again
    REQUIRE_THAT(listener.getLogTopicSubscriptions(), RangeEquals(std::map<std::string, Level>({{"FSM", Level::INFO}})));

    listener.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Extra topic subscriptions", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    sender.mockChirpService();

    // Pop CMD notification message from subscription at construction
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG?"));

    // Subscribe to extra topic
    listener.subscribeExtaLogTopic(to_string(sender.getName()), "FSM", Level::INFO);

    // Check subscription messages
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/INFO/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/WARNING/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/CRITICAL/FSM"));

    // Check extra log topic subscriptions
    REQUIRE_THAT(listener.getExtraLogTopicSubscriptions(to_string(sender.getName())),
                 RangeEquals(std::map<std::string, Level>({{"FSM", Level::INFO}})));

    // Unsubscribe from extra topic
    listener.unsubscribeExtraLogTopic(to_string(sender.getName()), "FSM");
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/INFO/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/WARNING/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/STATUS/FSM"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/CRITICAL/FSM"));

    listener.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("No empty topic subscription", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    sender.mockChirpService();

    // Pop CMD notification message from subscription at construction
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG?"));

    // Subscribe to empty topic
    listener.subscribeLogTopic("", Level::DEBUG);

    // Check that no subscription message is received
    REQUIRE_FALSE(sender.canRecv());

    // Check that subscription is not stored
    REQUIRE_THAT(listener.getLogTopicSubscriptions(), RangeEquals(std::map<std::string, Level>({})));

    listener.unsubscribeLogTopic("");
    listener.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Empty extra topic subscription", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto listener = LogListener("listener", {});
    listener.startPool();

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    sender.mockChirpService();

    // Pop CMD notification message from subscription at construction
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG?"));

    // Set global log topic
    listener.setGlobalLogLevel(Level::INFO);

    // Check subscription messages
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/INFO"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/WARNING"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/CRITICAL"));

    // Subscribe to empty topic for host
    listener.subscribeExtaLogTopic(to_string(sender.getName()), "", Level::TRACE);
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/TRACE"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/DEBUG"));

    // Increase empty topic for host
    listener.subscribeExtaLogTopic(to_string(sender.getName()), "", Level::WARNING);
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/TRACE"));
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/DEBUG"));

    // Increase global log level
    listener.setGlobalLogLevel(Level::STATUS);

    // Check extra log topic subscriptions
    REQUIRE_THAT(listener.getExtraLogTopicSubscriptions(to_string(sender.getName())),
                 RangeEquals(std::map<std::string, Level>({{"", Level::WARNING}})));

    listener.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}
