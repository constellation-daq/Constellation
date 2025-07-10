/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>

#include "constellation/controller/Controller.hpp"
#include "constellation/controller/exceptions.hpp"
#include "constellation/controller/MeasurementCondition.hpp"
#include "constellation/controller/MeasurementQueue.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
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

    const auto condition = std::make_shared<TimerCondition>(std::chrono::seconds(5));
    DummyQueue queue(controller, "queue_run_", condition);

    REQUIRE_THAT(condition->str(), Equals("Run for 5s"));

    REQUIRE_FALSE(queue.running());
    REQUIRE(queue.size() == 0);
    REQUIRE(queue.progress() == 0.);

    // Attempt to start, controller not in orbit:
    queue.start();
    REQUIRE_FALSE(queue.running());

    // Halt queue to check nothing happens
    queue.halt();

    // Stop controller
    controller.stop();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Missing Satellite in Queue", "[controller]") {
    // Create CHIRP manager for control service discovery
    create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    const auto condition = std::make_shared<TimerCondition>(std::chrono::seconds(5));
    DummyQueue queue(controller, "queue_run_", condition);

    // Create and start satellite
    DummySatellite satellite {"a"};
    satellite.skipTransitional(true);
    satellite.mockChirpService(CHIRP::CONTROL);
    satellite.mockChirpService(CHIRP::HEARTBEAT);

    // Await connection
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }
    controller.waitReachedState(CSCP::State::NEW, true);

    // Initialize and launch satellite, and check that state updates were propagated
    satellite.reactFSM(FSM::Transition::initialize, Configuration());
    controller.waitReachedState(CSCP::State::INIT, true);
    satellite.reactFSM(FSM::Transition::launch);
    controller.waitReachedState(CSCP::State::ORBIT, true);

    // Add measurements to the queue with unknown satellite
    const auto measurement = std::map<std::string, Controller::CommandPayload>({{"Dummy.b", Dictionary {}}});

    // Cannot add measurement, satellite is known
    REQUIRE_THROWS_MATCHES(queue.append(measurement),
                           QueueError,
                           Message("Measurement queue error: Satellite Dummy.b is unknown to controller"));

    // Stop controller
    controller.stop();
    satellite.exit();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Run Queue", "[controller]") {
    // Create CHIRP manager for control service discovery
    create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    const auto condition = std::make_shared<TimerCondition>(std::chrono::seconds(1));
    DummyQueue queue(controller, "queue_run_", condition);

    // Create and start satellite
    DummySatellite satellite {"a"};
    satellite.skipTransitional(true);
    satellite.mockChirpService(CHIRP::CONTROL);
    satellite.mockChirpService(CHIRP::HEARTBEAT);

    // Await connection
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }
    controller.waitReachedState(CSCP::State::NEW, true);

    // Initialize and launch satellite, and check that state updates were propagated
    satellite.reactFSM(FSM::Transition::initialize, Configuration());
    controller.waitReachedState(CSCP::State::INIT, true);
    satellite.reactFSM(FSM::Transition::launch);
    controller.waitReachedState(CSCP::State::ORBIT, true);

    // Add measurements to the queue
    const auto measurement = std::map<std::string, Controller::CommandPayload>({{"Dummy.a", Dictionary {}}});
    queue.append(measurement);
    queue.append(measurement);
    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::IDLE);
    REQUIRE(queue.size() == 2);
    REQUIRE_FALSE(queue.running());

    // Start the queue
    queue.start();
    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::RUNNING);
    REQUIRE(queue.running());

    // Stop queue, should end after current measurement
    queue.halt();
    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::IDLE);

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
    const auto condition = std::make_shared<TimerCondition>(std::chrono::seconds(10));
    DummyQueue queue(controller, "queue_run_", condition);

    // Create and start satellite
    DummySatellite satellite {"a"};
    satellite.skipTransitional(true);
    satellite.mockChirpService(CHIRP::CONTROL);
    satellite.mockChirpService(CHIRP::HEARTBEAT);

    // Await connection
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }
    controller.waitReachedState(CSCP::State::NEW, true);

    // Initialize and launch satellite, and check that state updates were propagated
    satellite.reactFSM(FSM::Transition::initialize, Configuration());
    controller.waitReachedState(CSCP::State::INIT, true);
    satellite.reactFSM(FSM::Transition::launch);
    controller.waitReachedState(CSCP::State::ORBIT, true);

    // Add measurements to the queue, overwriting default length
    const auto measurement_condition = std::make_shared<TimerCondition>(1s);
    const auto measurement = std::map<std::string, Controller::CommandPayload>({{"Dummy.a", Dictionary {}}});
    queue.append(measurement, measurement_condition);
    queue.append(measurement);
    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::IDLE);
    REQUIRE(queue.size() == 2);
    REQUIRE_FALSE(queue.running());

    // Start the queue and stop it directly, time should be below default duration of run
    auto timer = StopwatchTimer();
    queue.start();

    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::RUNNING);
    timer.start();

    REQUIRE(queue.running());
    queue.halt();

    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::IDLE);
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

    const auto condition = std::make_shared<TimerCondition>(std::chrono::seconds(1));
    DummyQueue queue(controller, "queue_run_", condition);

    // Create and start satellite
    DummySatellite satellite {"a"};
    satellite.skipTransitional(true);
    satellite.mockChirpService(CHIRP::CONTROL);
    satellite.mockChirpService(CHIRP::HEARTBEAT);

    // Await connection
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }
    controller.waitReachedState(CSCP::State::NEW, true);

    // Initialize and launch satellite, and check that state updates were propagated
    satellite.reactFSM(FSM::Transition::initialize, Configuration());
    controller.waitReachedState(CSCP::State::INIT, true);
    satellite.reactFSM(FSM::Transition::launch);
    controller.waitReachedState(CSCP::State::ORBIT, true);

    // Add measurements to the queue
    queue.append({{"Dummy.a", Dictionary {}}});
    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::IDLE);
    REQUIRE(queue.size() == 1);
    REQUIRE_FALSE(queue.running());

    // Start the queue
    queue.start();
    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::RUNNING);
    REQUIRE(queue.running());

    // Wait until in RUN state
    controller.awaitState(CSCP::State::RUN, 1s);

    // Interrupt directly
    queue.interrupt();
    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::IDLE);

    // Queue size after interrupting is still 1
    REQUIRE(queue.size() == 1);
    REQUIRE(queue.progress() == 0.);
    REQUIRE_FALSE(queue.running());
    REQUIRE(controller.getRunIdentifier() == "queue_run_0");

    // Restart the queue
    queue.start();
    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::RUNNING);
    REQUIRE(queue.running());

    // Wait until the queue successfully finished
    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::FINISHED);
    REQUIRE(queue.size() == 0);
    REQUIRE(queue.progress() == 1.);
    REQUIRE_FALSE(queue.running());
    REQUIRE(controller.getRunIdentifier() == "queue_run_0_retry_1");

    // Stop controller and exit satellites
    controller.stop();
    satellite.exit();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Clear Queue", "[controller]") {
    // Create CHIRP manager for control service discovery
    create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    const auto condition = std::make_shared<TimerCondition>(std::chrono::seconds(1));
    DummyQueue queue(controller, "queue_run_", condition);

    // Create and start satellite
    DummySatellite satellite {"a"};
    satellite.skipTransitional(true);
    satellite.mockChirpService(CHIRP::CONTROL);
    satellite.mockChirpService(CHIRP::HEARTBEAT);

    // Await connection
    while(controller.getConnectionCount() < 1) {
        std::this_thread::sleep_for(50ms);
    }
    controller.waitReachedState(CSCP::State::NEW, true);

    // Initialize and launch satellite, and check that state updates were propagated
    satellite.reactFSM(FSM::Transition::initialize, Configuration());
    controller.waitReachedState(CSCP::State::INIT, true);
    satellite.reactFSM(FSM::Transition::launch);
    controller.waitReachedState(CSCP::State::ORBIT, true);

    // Add measurements to the queue
    const auto measurement = std::map<std::string, Controller::CommandPayload>({{"Dummy.a", Dictionary {}}});
    queue.append(measurement);
    queue.append(measurement);
    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::IDLE);
    REQUIRE(queue.size() == 2);
    REQUIRE_FALSE(queue.running());

    // Start the queue
    queue.start();
    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::RUNNING);
    REQUIRE(queue.running());

    // Wait until in RUN state
    controller.awaitState(CSCP::State::RUN, 1s);

    // Start queue again to check nothing happens
    queue.start();
    REQUIRE(queue.running());

    // Clear queue while running, keeps current measurement
    queue.clear();
    REQUIRE(queue.running());
    REQUIRE(queue.size() == 1);

    // Stop queue, should end after current measurement
    queue.halt();
    queue.waitStateChanged();
    REQUIRE(queue.getState() == MeasurementQueue::State::FINISHED);

    REQUIRE(queue.size() == 0);
    REQUIRE(queue.progress() == 1.);
    REQUIRE_FALSE(queue.running());

    // Add new measurement and clear while queue is stopped
    queue.append(measurement);
    REQUIRE(queue.size() == 1);
    queue.clear();
    REQUIRE(queue.size() == 0);
    queue.clear();

    // Stop controller and exit satellites
    controller.stop();
    satellite.exit();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}
