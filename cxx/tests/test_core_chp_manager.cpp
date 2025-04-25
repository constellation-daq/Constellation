/**
 * @file
 * @brief Tests for CHP sender
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstddef>
#include <future>
#include <string>
#include <vector>

#include <asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/core/heartbeat/HeartbeatManager.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/timers.hpp"

#include "chp_mock.hpp"

using namespace constellation::heartbeat;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::chrono_literals;

class CHPManager : public HeartbeatManager {
public:
    CHPManager(std::string name)
        : HeartbeatManager(
              std::move(name),
              [&]() {
                  const std::lock_guard lock {mutex_};
                  return state_;
              },
              [&](std::string_view status) {
                  const std::lock_guard lock {mutex_};
                  last_interrupt_message_ = std::string(status);
              }) {}

    void setState(CSCP::State state) {
        const std::lock_guard lock {mutex_};
        state_ = state;
    }

    std::string_view getInterruptMessage() {
        const std::lock_guard lock {mutex_};
        return last_interrupt_message_;
    }

private:
    std::mutex mutex_;
    CSCP::State state_ {CSCP::State::NEW};
    std::string last_interrupt_message_;
};

TEST_CASE("Check remote state", "[chp][send]") {
    create_chirp_manager();

    auto manager = CHPManager("mgr");

    // Remote is not known:
    REQUIRE_FALSE(manager.getRemoteState("sender").has_value());

    auto sender = CHPMockSender("sender");
    sender.mockChirpService();
    sender.sendHeartbeat(CSCP::State::ORBIT, std::chrono::milliseconds(100000));
    std::this_thread::sleep_for(100ms);

    // Remote is known
    REQUIRE(manager.getRemoteState("sender").has_value());
    REQUIRE(manager.getRemoteState("sender").value() == CSCP::State::ORBIT);

    manager.terminate();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}
