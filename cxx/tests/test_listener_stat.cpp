/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <set>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/listener/StatListener.hpp"

#include "chirp_mock.hpp"
#include "cmdp_mock.hpp"

using namespace Catch::Matchers;
using namespace constellation::listener;
using namespace constellation::log;
using namespace constellation::protocol;
using namespace constellation::utils;

TEST_CASE("Metric subscriptions", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto listener = StatListener("listener", {});
    listener.startPool();

    // Subscribe to topic
    listener.subscribeMetric("FOO");

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    sender.mockChirpService();

    // Pop subscription messages (note: subscriptions come alphabetically if iterated from set)
    REQUIRE(check_sub_message(sender.recv().pop(), true, "STAT/FOO"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "STAT?"));

    // Subscribe to new topic
    listener.subscribeMetric("BAR");
    REQUIRE(check_sub_message(sender.recv().pop(), true, "STAT/BAR"));

    // Check subscribed topics
    REQUIRE_THAT(listener.getMetricSubscriptions(), RangeEquals(std::set<std::string>({"FOO", "BAR"})));

    // Unsubscribe from a topic
    listener.unsubscribeMetric("FOO");
    REQUIRE(check_sub_message(sender.recv().pop(), false, "STAT/FOO"));

    // Check subscribed topics again
    REQUIRE_THAT(listener.getMetricSubscriptions(), RangeEquals(std::set<std::string>({"BAR"})));

    listener.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Per-host metric topic subscriptions", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto listener = StatListener("listener", {});
    listener.startPool();

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    sender.mockChirpService();

    // Pop CMD notification message from subscription at construction
    REQUIRE(check_sub_message(sender.recv().pop(), true, "STAT?"));

    // Subscribe to per-host metric
    listener.subscribeMetric(to_string(sender.getName()), "FOO");

    // Check subscription messages
    REQUIRE(check_sub_message(sender.recv().pop(), true, "STAT/FOO"));

    // Check metric subscriptions
    REQUIRE_THAT(listener.getMetricSubscriptions(to_string(sender.getName())), RangeEquals(std::set<std::string>({"FOO"})));

    // Unsubscribe from extra topic
    listener.unsubscribeMetric(to_string(sender.getName()), "FOO");
    REQUIRE(check_sub_message(sender.recv().pop(), false, "STAT/FOO"));

    listener.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Empty metric subscription", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto listener = StatListener("listener", {});
    listener.startPool();

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    sender.mockChirpService();

    // Pop CMD notification message from subscription at construction
    REQUIRE(check_sub_message(sender.recv().pop(), true, "STAT?"));

    // Subscribe to empty topic
    listener.subscribeMetric("");

    // Check that subscription message for any topic is received
    REQUIRE(check_sub_message(sender.recv().pop(), true, "STAT/"));

    // Check that subscription is not stored
    REQUIRE_THAT(listener.getMetricSubscriptions(), RangeEquals(std::set<std::string>({""})));

    listener.unsubscribeMetric("");
    listener.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}
