/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <thread>
#include <tuple>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>

#include "constellation/controller/Controller.hpp"
#include "constellation/controller/MeasurementQueue.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/FSM.hpp"

#include "chirp_mock.hpp"
#include "dummy_controller.hpp"
#include "dummy_satellite.hpp"

using namespace Catch::Matchers;
using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;

TEST_CASE("Empty Queue", "[controller]") {
    // Create CHIRP manager for control service discovery
    create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    DummyQueue queue(controller, "queue_run_", {std::chrono::seconds(5)});

    REQUIRE_FALSE(queue.running());
    REQUIRE(queue.size() == 0);
    REQUIRE(queue.progress() == 0.);

    // Attempt to start, controller not in orbit:
    queue.start();
    REQUIRE_FALSE(queue.running());

    // Stop controller
    controller.stop();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Run Queue", "[controller]") {
    // Create CHIRP manager for control service discovery
    create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    DummyQueue queue(controller, "queue_run_", {std::chrono::seconds(1)});

    // Create and start satellite
    DummySatellite satellite {"a"};
    satellite.mockChirpService(CHIRP::CONTROL);
    satellite.mockChirpService(CHIRP::HEARTBEAT);

    // Await connection
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }

    // Initialize and launch satellite, and check that state updates were propagated
    satellite.reactFSM(FSM::Transition::initialize, Configuration());
    controller.waitReachedState(CSCP::State::INIT, true);
    satellite.reactFSM(FSM::Transition::launch);
    controller.waitReachedState(CSCP::State::ORBIT, true);

    // Add measurements to the queue
    const auto measurement = std::map<std::string, Controller::CommandPayload>({{"b", Dictionary {}}});
    queue.append(measurement);
    queue.append(measurement);
    REQUIRE(queue.size() == 2);
    REQUIRE_FALSE(queue.running());

    // Start the queue and stop it directly, should end after current measurement
    queue.start();
    satellite.progressFsm();

    queue.waitStarted();
    REQUIRE(queue.running());
    queue.halt();
    satellite.progressFsm();

    queue.waitStopped();
    REQUIRE(queue.size() == 1);
    REQUIRE(queue.progress() == 0.5);
    REQUIRE_FALSE(queue.running());

    // Stop controller and exit satellites
    controller.stop();
    satellite.exit();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Set per-measurement conditions", "[controller]") {

    // Create CHIRP manager for control service discovery
    create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // Very long default duration
    DummyQueue queue(controller, "queue_run_", {std::chrono::seconds(10)});

    // Create and start satellite
    DummySatellite satellite {"a"};
    satellite.mockChirpService(CHIRP::CONTROL);
    satellite.mockChirpService(CHIRP::HEARTBEAT);

    // Await connection
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }

    // Initialize and launch satellite, and check that state updates were propagated
    satellite.reactFSM(FSM::Transition::initialize, Configuration());
    controller.waitReachedState(CSCP::State::INIT, true);
    satellite.reactFSM(FSM::Transition::launch);
    controller.waitReachedState(CSCP::State::ORBIT, true);

    // Add measurements to the queue, overwriting default length
    const auto measurement = std::map<std::string, Controller::CommandPayload>({{"b", Dictionary {}}});
    queue.append(measurement, {std::chrono::seconds(1)});
    queue.append(measurement);
    REQUIRE(queue.size() == 2);
    REQUIRE_FALSE(queue.running());

    // Start the queue and stop it directly, time should be below default duration of run
    auto timer = StopwatchTimer();
    queue.start();
    satellite.progressFsm();

    queue.waitStarted();
    timer.start();

    REQUIRE(queue.running());
    queue.halt();
    satellite.progressFsm();

    queue.waitStopped();
    timer.stop();

    REQUIRE(queue.size() == 1);
    REQUIRE(timer.duration() < 2s);
    REQUIRE(queue.progress() == 0.5);
    REQUIRE_FALSE(queue.running());

    // Stop controller and exit satellites
    controller.stop();
    satellite.exit();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Interrupt Queue", "[controller]") {
    // Create CHIRP manager for control service discovery
    create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    DummyQueue queue(controller, "queue_run_", {std::chrono::seconds(1)});

    // Create and start satellite
    DummySatellite satellite {"a"};
    satellite.mockChirpService(CHIRP::CONTROL);
    satellite.mockChirpService(CHIRP::HEARTBEAT);

    // Await connection
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }

    // Initialize and launch satellite, and check that state updates were propagated
    satellite.reactFSM(FSM::Transition::initialize, Configuration());
    controller.waitReachedState(CSCP::State::INIT, true);
    satellite.reactFSM(FSM::Transition::launch);
    controller.waitReachedState(CSCP::State::ORBIT, true);

    // Add measurements to the queue
    queue.append({{"b", Dictionary {}}});
    REQUIRE(queue.size() == 1);
    REQUIRE_FALSE(queue.running());

    // Start the queue and interrupt it directly
    queue.start();
    satellite.progressFsm();

    queue.waitStarted();
    REQUIRE(queue.running());
    queue.interrupt();
    satellite.progressFsm();
    queue.waitStopped();

    // Queue size after interrupting is still 1
    REQUIRE(queue.size() == 1);
    REQUIRE(queue.progress() == 0.);
    REQUIRE_FALSE(queue.running());

    // Restart the queue
    queue.start();
    satellite.progressFsm();

    queue.waitStarted();
    REQUIRE(queue.running());
    controller.waitReachedState(CSCP::State::stopping, true);
    satellite.progressFsm();

    queue.waitStopped();
    REQUIRE(queue.size() == 0);
    REQUIRE(queue.progress() == 1.);
    REQUIRE_FALSE(queue.running());

    // Stop controller and exit satellites
    controller.stop();
    satellite.exit();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}
