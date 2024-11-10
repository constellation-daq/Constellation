/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <cstddef>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <msgpack.hpp>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/controller/Controller.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/Satellite.hpp"

#include "chirp_mock.hpp"
#include "dummy_controller.hpp"
#include "dummy_satellite.hpp"

using namespace Catch::Matchers;
using namespace constellation;
using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Satellite connecting", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // No connections at present:
    REQUIRE(controller.getConnectionCount() == 0);

    // Create and start satellite
    const DummySatellite satellite {};
    chirp_mock_service("Dummy.sat1", chirp::CONTROL, satellite.getCommandPort());

    // Check that satellite connected
    REQUIRE(controller.getConnectionCount() == 1);
    REQUIRE(controller.getConnections().contains("Dummy.sat1"));
    REQUIRE(controller.isInState(CSCP::State::NEW));

    // Stop the controller:
    controller.stop();
    REQUIRE(controller.getConnectionCount() == 0);
}

TEST_CASE("Attempt connection from satellites with same canonical name", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // No connections at present:
    REQUIRE(controller.getConnectionCount() == 0);

    // Create and start satellite
    const DummySatellite satellite1 {"a"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satellite1.getCommandPort());

    // Check that satellite connected
    REQUIRE(controller.getConnectionCount() == 1);
    REQUIRE(controller.getConnections().contains("Dummy.a"));
    REQUIRE(controller.isInState(CSCP::State::NEW));

    // Create and start second satellite with same canonical name
    const DummySatellite satellite2 {"a"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satellite2.getCommandPort());

    // Check that second satellite was not connected
    REQUIRE(controller.getConnectionCount() == 1);

    // Stop the controller:
    controller.stop();
    REQUIRE(controller.getConnectionCount() == 0);
}

TEST_CASE("Satellite departing", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // No connections at present:
    REQUIRE(controller.getConnectionCount() == 0);

    // Create and start satellite
    const DummySatellite satellite {};
    chirp_mock_service("Dummy.sat1", chirp::CONTROL, satellite.getCommandPort());

    // Check that satellite connected
    REQUIRE(controller.getConnectionCount() == 1);
    REQUIRE(controller.getConnections().contains("Dummy.sat1"));
    REQUIRE(controller.isInState(CSCP::State::NEW));

    // Depart the satellite
    chirp_mock_service("Dummy.sat1", chirp::CONTROL, satellite.getCommandPort(), false);

    // Wait for CHIRP message to be processed:
    std::this_thread::sleep_for(100ms);
    REQUIRE(controller.getConnectionCount() == 0);
}

TEST_CASE("State Updates are propagated", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // Create and start satellite
    const DummySatellite satellite {"a"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satellite.getCommandPort());

    // Check that state updates were propagated:
    REQUIRE(controller.last_reached_state() == std::tuple<CSCP::State, bool> {CSCP::State::NEW, true});
    const auto [type, position, size] = controller.last_propagate_update();
    // FIXME protected
    // REQUIRE(type == Controller::UpdateType::ADDED);
    REQUIRE(position == 0);
    REQUIRE(size == 1);

    // Create and start second satellite
    const DummySatellite satellite2 {"z"};
    chirp_mock_service("Dummy.z", chirp::CONTROL, satellite2.getCommandPort());

    // Check that state updates were propagated:
    REQUIRE(controller.last_reached_state() == std::tuple<CSCP::State, bool> {CSCP::State::NEW, true});
    const auto [type2, position2, size2] = controller.last_propagate_update();
    // FIXME protected
    // REQUIRE(type == Controller::UpdateType::ADDED);
    REQUIRE(position2 == 1);
    REQUIRE(size2 == 2);

    // Stop the controller:
    controller.stop();
    REQUIRE(controller.getConnectionCount() == 0);
}

TEST_CASE("Satellite state updates are received", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // Create and start satellite
    DummySatellite satellite {"a"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satellite.getCommandPort());
    chirp_mock_service("Dummy.a", chirp::HEARTBEAT, satellite.getHeartbeatPort());

    // Check that state updates were propagated:
    REQUIRE(controller.last_reached_state() == std::tuple<CSCP::State, bool> {CSCP::State::NEW, true});

    // Initialize satellite
    satellite.reactFSM(FSM::Transition::initialize, Configuration());

    // Check that state updates were received:
    REQUIRE(controller.last_reached_state() == std::tuple<CSCP::State, bool> {CSCP::State::INIT, true});

    // Stop the controller:
    controller.stop();
    REQUIRE(controller.getConnectionCount() == 0);
}

TEST_CASE("Mixed and global states are reported", "[controller]") {
    // Create CHIRP manager for control service discovery
    auto chirp_manager = create_chirp_manager();

    // Create and start controller
    DummyController controller {"ctrl"};
    controller.start();

    // Create and start satellite
    DummySatellite satelliteA {"a"};
    DummySatellite satelliteB {"b"};
    chirp_mock_service("Dummy.a", chirp::CONTROL, satelliteA.getCommandPort());
    chirp_mock_service("Dummy.a", chirp::HEARTBEAT, satelliteA.getHeartbeatPort());
    chirp_mock_service("Dummy.b", chirp::CONTROL, satelliteB.getCommandPort());
    chirp_mock_service("Dummy.b", chirp::HEARTBEAT, satelliteB.getHeartbeatPort());

    // Check that state updates were propagated:
    REQUIRE(controller.last_reached_state() == std::tuple<CSCP::State, bool> {CSCP::State::NEW, true});
    REQUIRE(controller.getLowestState() == CSCP::State::NEW);
    REQUIRE(controller.isInGlobalState());

    // Initialize satelliteA
    satelliteA.reactFSM(FSM::Transition::initialize, Configuration());
    std::this_thread::sleep_for(100ms);

    // Check that state is mentioned as mixed:
    REQUIRE(controller.last_reached_state() == std::tuple<CSCP::State, bool> {CSCP::State::NEW, false});
    REQUIRE(controller.getLowestState() == CSCP::State::NEW);
    REQUIRE_FALSE(controller.isInGlobalState());

    // Initialize satelliteB
    satelliteB.reactFSM(FSM::Transition::initialize, Configuration());
    std::this_thread::sleep_for(100ms);

    // Check that state is mentioned as mixed:
    REQUIRE(controller.last_reached_state() == std::tuple<CSCP::State, bool> {CSCP::State::INIT, true});
    REQUIRE(controller.getLowestState() == CSCP::State::INIT);
    REQUIRE(controller.isInGlobalState());

    // Stop the controller:
    controller.stop();
    REQUIRE(controller.getConnectionCount() == 0);
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
