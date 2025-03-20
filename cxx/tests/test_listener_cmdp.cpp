/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono> // IWYU pragma: keep
#include <map>
#include <set>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/listener/CMDPListener.hpp"

#include "chirp_mock.hpp"
#include "cmdp_mock.hpp"
#include "dummy_listener.hpp"

using namespace Catch::Matchers;
using namespace constellation::listener;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::metrics;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::chrono_literals;

TEST_CASE("Changing subscriptions", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto pool = CMDPListener("pool", {});
    pool.startPool();

    // Set subscription topics
    pool.multiscribeTopics({}, {"LOG/STATUS", "LOG/INFO"});

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    sender.mockChirpService();

    // Pop subscription messages (note: subscriptions come alphabetically if iterated from set)
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/INFO"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS"));

    // Check topic subscriptions
    REQUIRE_THAT(pool.getTopicSubscriptions(), RangeEquals(std::set<std::string>({"LOG/STATUS", "LOG/INFO"})));

    // Unsubscribe from topic
    pool.unsubscribeTopic("LOG/INFO");
    REQUIRE(check_sub_message(sender.recv().pop(), false, "LOG/INFO"));

    // No non-subscribed unsubscriptions
    pool.unsubscribeTopic("LOG/INFO");
    pool.unsubscribeTopic("LOG/NOTSUBSCRIBED");
    REQUIRE_FALSE(sender.canRecv());

    // Subscribe to new topic
    pool.subscribeTopic("LOG/TRACE");
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/TRACE"));

    // No duplicate subscriptions
    pool.subscribeTopic("LOG/TRACE");
    REQUIRE_FALSE(sender.canRecv());

    // Check topic subscriptions again
    REQUIRE_THAT(pool.getTopicSubscriptions(), RangeEquals(std::set<std::string>({"LOG/STATUS", "LOG/TRACE"})));

    pool.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Changing extra subscriptions", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto pool = CMDPListener("pool", {});
    pool.startPool();

    // Set subscription topics
    pool.multiscribeTopics({}, {"LOG/STATUS", "LOG/INFO"});

    // Start the senders and mock via chirp
    auto sender1 = CMDPSender("CMDPSender.s1");
    sender1.mockChirpService();
    auto sender2 = CMDPSender("CMDPSender.s2");
    sender2.mockChirpService();

    // Pop subscription messages (note: subscriptions come alphabetically if iterated from set)
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "LOG/INFO"));
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "LOG/STATUS"));
    REQUIRE(check_sub_message(sender2.recv().pop(), true, "LOG/INFO"));
    REQUIRE(check_sub_message(sender2.recv().pop(), true, "LOG/STATUS"));

    // Check no extra topic subscriptions yet
    REQUIRE_THAT(pool.getExtraTopicSubscriptions(to_string(sender1.getName())), RangeEquals(std::set<std::string>({})));

    // Add extra subscription: s1 now at LOG/STATUS, LOG/INFO, LOG/TRACE
    pool.subscribeExtraTopic(to_string(sender1.getName()), "LOG/TRACE");

    // Check subscription message
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "LOG/TRACE"));

    // No duplicate extra subscriptions
    pool.subscribeExtraTopic(to_string(sender1.getName()), "LOG/TRACE");
    REQUIRE_FALSE(sender1.canRecv());

    // Additional extra subscription
    pool.subscribeExtraTopic(to_string(sender1.getName()), "LOG/WARNING");
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "LOG/WARNING"));

    // Check extra topic subscriptions
    REQUIRE_THAT(pool.getExtraTopicSubscriptions(to_string(sender1.getName())),
                 RangeEquals(std::set<std::string>({"LOG/TRACE", "LOG/WARNING"})));

    // Unsubscribe again
    pool.unsubscribeExtraTopic(to_string(sender1.getName()), "LOG/WARNING");
    REQUIRE(check_sub_message(sender1.recv().pop(), false, "LOG/WARNING"));

    // Replace extra subscription: s1 now at LOG/STATUS, LOG/INFO, LOG/DEBUG
    pool.multiscribeExtraTopics(to_string(sender1.getName()), {"LOG/TRACE"}, {"LOG/DEBUG", "LOG/INFO"});

    // Check changing subscriptions
    REQUIRE(check_sub_message(sender1.recv().pop(), false, "LOG/TRACE"));
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "LOG/DEBUG"));

    // Unsubscribe from LOG/INFO for all
    pool.unsubscribeTopic("LOG/INFO");
    REQUIRE(check_sub_message(sender1.recv().pop(), false, "LOG/INFO"));
    REQUIRE(check_sub_message(sender2.recv().pop(), false, "LOG/INFO"));

    // Check that sender1 gets subscription again since extra topic
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "LOG/INFO"));

    // Check extra topic subscriptions again
    REQUIRE_THAT(pool.getExtraTopicSubscriptions(to_string(sender1.getName())),
                 RangeEquals(std::set<std::string>({"LOG/DEBUG", "LOG/INFO"})));

    // Remove extra subscriptions
    pool.removeExtraTopicSubscriptions(to_string(sender1.getName()));
    REQUIRE(check_sub_message(sender1.recv().pop(), false, "LOG/DEBUG"));

    pool.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Extra subscriptions on connection", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto pool = CMDPListener("pool", {});
    pool.startPool();

    // Set subscription topics
    pool.multiscribeTopics({}, {"LOG/STATUS", "LOG/INFO"});
    pool.multiscribeExtraTopics("CMDPSender.s1", {}, {"LOG/INFO", "SOMETHING", "ELSE"});
    pool.unsubscribeExtraTopic("CMDPSender.s1", "ELSE");

    // Start the senders and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    sender.mockChirpService();

    // Pop subscription messages for global subscriptions (note: subscriptions come alphabetically if iterated from set)
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/INFO"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "LOG/STATUS"));

    // Check extra subscription message
    REQUIRE(check_sub_message(sender.recv().pop(), true, "SOMETHING"));

    // Remove all extra subscriptions
    pool.removeExtraTopicSubscriptions();

    // Check unsubscription message
    REQUIRE(check_sub_message(sender.recv().pop(), false, "SOMETHING"));

    pool.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Available topics", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto pool = DummyListener();
    pool.startPool();

    // Set subscription topics
    pool.multiscribeExtraTopics("CMDPSender.s1", {}, {"STAT?", "STAT/"});
    pool.multiscribeExtraTopics("CMDPSender.s2", {}, {"LOG?", "LOG/"});

    // Start the sender and mock via chirp
    auto sender1 = CMDPSender("CMDPSender.s1");
    sender1.mockChirpService();
    auto sender2 = CMDPSender("CMDPSender.s2");
    sender2.mockChirpService();

    // Pop subscription messages (note: subscriptions come alphabetically if iterated from set)
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "STAT/"));
    REQUIRE(check_sub_message(sender1.recv().pop(), true, "STAT?"));
    REQUIRE(check_sub_message(sender2.recv().pop(), true, "LOG/"));
    REQUIRE(check_sub_message(sender2.recv().pop(), true, "LOG?"));

    // No topics available before first message
    REQUIRE_FALSE(pool.isTopicAvailable("STAT/C"));
    REQUIRE(pool.getAvailableTopics("CMDPSender.s1").empty());

    // Send a message to trigger manual topic addition
    sender1.sendStatMessage("STAT/C", "c", MetricType::LAST_VALUE, 5);
    pool.popNextMessage();

    // Check that STAT/C was added (without description)
    REQUIRE(pool.isTopicAvailable("STAT/C"));
    REQUIRE(pool.getAvailableTopics().at("STAT/C").empty());

    // Send a notification to trigger topic addition
    sender1.sendNotification("STAT?", {{{"STAT/A", "A"}, {"STAT/B", "B"}}});

    // Send a message to ensure notification was received
    sender1.sendStatMessage("STAT/C", "c", MetricType::LAST_VALUE, 6);
    pool.popNextMessage();

    // Check that STAT/A and STAT/B were added
    REQUIRE_THAT(
        pool.getAvailableTopics("CMDPSender.s1"),
        UnorderedRangeEquals(std::map<std::string, std::string>({{"STAT/A", "A"}, {"STAT/B", "B"}, {"STAT/C", ""}})));

    // Send notification first
    sender2.sendNotification("LOG?", {{{"LOG/A", "A"}, {"LOG/B", "B"}}});

    // Send a message to ensure notification was received
    sender2.sendLogMessage(Level::STATUS, "LOG/A", "A");
    pool.popNextMessage();

    // Check topics
    REQUIRE_THAT(pool.getAvailableTopics(),
                 UnorderedRangeEquals(std::map<std::string, std::string>(
                     {{"LOG/A", "A"}, {"LOG/B", "B"}, {"STAT/A", "A"}, {"STAT/B", "B"}, {"STAT/C", ""}})));

    pool.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Available senders", "[listener]") {
    // Create CHIRP manager for monitoring service discovery
    create_chirp_manager();

    // Start pool
    auto pool = DummyListener();
    pool.startPool();

    // Set subscription topics
    pool.multiscribeTopics({}, {"STAT?", "STAT/"});

    // Start the sender and mock via chirp
    auto sender = CMDPSender("CMDPSender.s1");
    chirp_mock_service(sender.getName(), CHIRP::MONITORING, sender.getPort());

    // Pop subscription messages (note: subscriptions come alphabetically if iterated from set)
    REQUIRE(check_sub_message(sender.recv().pop(), true, "STAT/"));
    REQUIRE(check_sub_message(sender.recv().pop(), true, "STAT?"));

    // No senders available before first message
    REQUIRE_FALSE(pool.isSenderAvailable("CMDPSender.s1"));

    // Send a message
    sender.sendStatMessage("STAT/C", "c", MetricType::LAST_VALUE, 7);
    pool.popNextMessage();

    // Check that sender is available
    REQUIRE(pool.isSenderAvailable("CMDPSender.s1"));

    // Check that topic exists
    REQUIRE(pool.getAvailableTopics().size() == 1);

    // Depart sender
    chirp_mock_service(sender.getName(), CHIRP::MONITORING, sender.getPort(), false);

    // Wait until CHIRP message is processed
    while(pool.countSockets() > 0) {
        std::this_thread::sleep_for(50ms);
    }

    // Check that sender is gone
    REQUIRE_FALSE(pool.isSenderAvailable("CMDPSender.s1"));

    // Check that topic is gone
    REQUIRE(pool.getAvailableTopics().empty());

    pool.stopPool();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}
