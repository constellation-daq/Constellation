/**
 * @file
 * @brief Tests for CHP sender
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <utility>

#include <asio.hpp>
#include <catch2/catch_test_macros.hpp>

#include "constellation/core/heartbeat/HeartbeatSend.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/timers.hpp"

#include "chirp_mock.hpp"
#include "chp_mock.hpp"

using namespace constellation::heartbeat;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::chrono_literals;

class CHPSender : public HeartbeatSend {
public:
    CHPSender(std::string name, std::chrono::milliseconds interval)
        : HeartbeatSend(std::move(name), [&]() { return state_.load(); }, interval) {}

    void setState(CSCP::State state) { state_ = state; }

private:
    std::atomic<CSCP::State> state_ {CSCP::State::NEW};
};

TEST_CASE("Send a heartbeat", "[chp][send]") {
    create_chirp_manager();

    auto receiver = CHPMockReceiver();
    receiver.startPool();

    auto timer = StopwatchTimer();
    const auto interval = std::chrono::milliseconds(300);
    auto sender = CHPSender("Sender", interval);

    // Mock service and wait until subscribed
    const auto mocked_service = MockedChirpService("Sender", CHIRP::ServiceIdentifier::HEARTBEAT, sender.getPort());
    receiver.waitSubscription();

    // Wait for first message
    receiver.waitNextMessage();

    // Start timer and wait for the next:
    timer.start();
    receiver.waitNextMessage();
    timer.stop();

    // The delay should have been less than the configured interval:
    REQUIRE(std::chrono::duration_cast<std::chrono::milliseconds>(timer.duration()) < interval);

    // Check that heartbeat is decoded correctly
    const auto last_message = receiver.getLastMessage();
    REQUIRE(last_message->getSender() == "Sender");
    REQUIRE(last_message->getState() == CSCP::State::NEW);
    REQUIRE_FALSE(last_message->getStatus().has_value());
    REQUIRE(last_message->getInterval() == interval);

    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
    sender.terminate();
    receiver.stopPool();
}

TEST_CASE("Send an extrasystole", "[chp][send]") {
    create_chirp_manager();

    auto receiver = CHPMockReceiver();
    receiver.startPool();

    const auto interval = std::chrono::milliseconds(300);
    auto sender = CHPSender("Sender", interval);

    // Mock service and wait until subscribed
    const auto mocked_service = MockedChirpService("Sender", CHIRP::ServiceIdentifier::HEARTBEAT, sender.getPort());
    receiver.waitSubscription();

    // Set state and send extrasystole
    sender.setState(CSCP::State::RUN);
    sender.sendExtrasystole("test");

    // Wait until heartbeat is received
    receiver.waitNextMessage();

    // Check that extrasystole is decoded correctly
    const auto extra_message = receiver.getLastMessage();
    REQUIRE(extra_message->getSender() == "Sender");
    REQUIRE(extra_message->getState() == CSCP::State::RUN);
    REQUIRE(extra_message->getStatus().has_value());
    REQUIRE(extra_message->getStatus().value() == "test"); // NOLINT(bugprone-unchecked-optional-access)
    REQUIRE(extra_message->getInterval() == interval);

    // Wait until heartbeat is received
    receiver.waitNextMessage();

    // Check that heartbeat is decoded correctly
    const auto next_message = receiver.getLastMessage();
    REQUIRE(next_message->getSender() == "Sender");
    REQUIRE(next_message->getState() == CSCP::State::RUN);
    REQUIRE_FALSE(next_message->getStatus().has_value());
    REQUIRE(next_message->getInterval() == interval);

    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
    sender.terminate();
    receiver.stopPool();
}

TEST_CASE("Change heartbeat interval", "[chp][send]") {
    create_chirp_manager();

    auto receiver = CHPMockReceiver();
    receiver.startPool();

    auto timer = StopwatchTimer();
    auto sender = CHPSender("Sender", std::chrono::milliseconds(200));

    // Mock service and wait until subscribed
    const auto mocked_service = MockedChirpService("Sender", CHIRP::ServiceIdentifier::HEARTBEAT, sender.getPort());
    receiver.waitSubscription();

    // Wait for first message
    receiver.waitNextMessage();

    // Start timer and wait for the next:
    timer.start();
    receiver.waitNextMessage();
    timer.stop();

    // The delay should have been less than the configured interval:
    REQUIRE(std::chrono::duration_cast<std::chrono::milliseconds>(timer.duration()) < std::chrono::milliseconds(200));

    // Change interval:
    sender.updateInterval(std::chrono::milliseconds(600));

    // Wait for first message
    receiver.waitNextMessage();

    // Start timer and wait for the next:
    timer.start();
    receiver.waitNextMessage();
    timer.stop();

    // The delay should have been less than the new but more than the previous interval:
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(timer.duration());
    REQUIRE(duration > std::chrono::milliseconds(200));
    REQUIRE(duration < std::chrono::milliseconds(600));

    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
    sender.terminate();
    receiver.stopPool();
}
