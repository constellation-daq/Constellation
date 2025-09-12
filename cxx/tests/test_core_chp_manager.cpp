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
#include <string_view>
#include <thread>
#include <utility>

#include <asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/core/heartbeat/HeartbeatManager.hpp"
#include "constellation/core/protocol/CHP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"

#include "chirp_mock.hpp"
#include "chp_mock.hpp"

using namespace Catch::Matchers;
using namespace constellation::heartbeat;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::chrono_literals;

class CHPManager : public HeartbeatManager {
public:
    CHPManager(std::string name)
        : HeartbeatManager(
              std::move(name),
              [&]() { return CSCP::State::NEW; },
              [&](std::string_view status) {
                  const std::lock_guard lock {mutex_};
                  interrupt_message_ = std::string(status);
                  interrupt_received_.store(true);
              },
              [&](std::string_view /*reason*/) {
                  const std::lock_guard lock {mutex_};
                  degraded_received_.store(true);
              }) {}

    void waitInterrupt() {
        while(!interrupt_received_.load()) {
            std::this_thread::yield();
        }
        interrupt_received_.store(false);
    }

    void waitDegraded() {
        while(!degraded_received_.load()) {
            std::this_thread::yield();
        }
        degraded_received_.store(false);
    }

    std::string getInterruptMessage() {
        const std::lock_guard lock {mutex_};
        return interrupt_message_;
    }

private:
    std::atomic_bool degraded_received_ {false};
    std::atomic_bool interrupt_received_ {false};
    std::mutex mutex_;
    std::string interrupt_message_;
};

TEST_CASE("Check remote state", "[chp][send]") {
    create_chirp_manager();

    auto manager = CHPManager("mgr");

    // Remote is not known:
    REQUIRE_FALSE(manager.getRemoteState("sender").has_value());

    auto sender = CHPMockSender("sender");
    sender.mockChirpOffer();

    while(!manager.getRemoteState(sender.getName()).has_value()) {
        sender.sendHeartbeat(CSCP::State::ORBIT, std::chrono::milliseconds(100000));
        std::this_thread::sleep_for(50ms);
    }

    // Remote is known
    REQUIRE(manager.getRemoteState(sender.getName()).has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    REQUIRE(manager.getRemoteState(sender.getName()).value() == CSCP::State::ORBIT);

    // Depart with the sender
    sender.mockChirpDepart();
    while(manager.getRemoteState(sender.getName()).has_value()) {
        std::this_thread::yield();
    }

    // Remote is not known:
    REQUIRE_FALSE(manager.getRemoteState(sender.getName()).has_value());

    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
    manager.terminate();
}

TEST_CASE("Receive interrupt from failure states", "[chp][send]") {
    create_chirp_manager();

    auto manager = CHPManager("mgr");
    auto sender = CHPMockSender("sender");
    sender.mockChirpOffer();

    while(!manager.getRemoteState(sender.getName()).has_value()) {
        sender.sendHeartbeat(CSCP::State::ORBIT, std::chrono::milliseconds(100000));
        std::this_thread::sleep_for(50ms);
    }

    // Send heartbeat with ERROR state
    sender.sendHeartbeat(CSCP::State::ERROR, std::chrono::milliseconds(100000), CHP::flags_from_role(CHP::Role::DYNAMIC));

    // Wait for interrupt
    manager.waitInterrupt();
    REQUIRE_THAT(manager.getInterruptMessage(), Equals("`sender` reports state ERROR"));

    // Clear remote error state by sending heartbeat with regular state:
    sender.sendHeartbeat(CSCP::State::INIT, std::chrono::milliseconds(100000), CHP::flags_from_role(CHP::Role::DYNAMIC));

    // Send heartbeat with SAFE state
    sender.sendHeartbeat(CSCP::State::SAFE, std::chrono::milliseconds(100000), CHP::flags_from_role(CHP::Role::DYNAMIC));

    // Wait for interrupt
    manager.waitInterrupt();
    REQUIRE_THAT(manager.getInterruptMessage(), Equals("`sender` reports state SAFE"));

    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
    manager.terminate();
}

TEST_CASE("Receive interrupt from heartbeat timeout", "[chp][send]") {
    create_chirp_manager();

    auto manager = CHPManager("mgr");
    auto sender = CHPMockSender("sender");
    sender.mockChirpOffer();

    // Send heartbeat with NEW state to register the remote
    while(!manager.getRemoteState(sender.getName()).has_value()) {
        std::this_thread::sleep_for(50ms);
        sender.sendHeartbeat(CSCP::State::NEW, std::chrono::milliseconds(100), CHP::flags_from_role(CHP::Role::DYNAMIC));
    }

    // Wait for interrupt
    manager.waitInterrupt();
    REQUIRE_THAT(manager.getInterruptMessage(), Equals("No signs of life detected anymore from `sender`"));

    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
    manager.terminate();
}
