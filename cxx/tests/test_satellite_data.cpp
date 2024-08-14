/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <chrono>
#include <thread>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

#include "dummy_satellite.hpp"

using namespace constellation::config;
using namespace constellation::satellite;
using namespace std::literals::chrono_literals;

class Transmitter : public DummySatellite<TransmitterSatellite> {};

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Transmitter / BOR timeout", "[satellite]") {
    auto transmitter = Transmitter();
    auto config = Configuration();
    config.set("_data_bor_timeout", 1);
    transmitter.getFSM().react(FSM::Transition::initialize, std::move(config));
    transmitter.progressFsm();
    transmitter.getFSM().react(FSM::Transition::launch);
    transmitter.progressFsm();
    transmitter.getFSM().react(FSM::Transition::start, "test");
    transmitter.progressFsm();
    // Give some time for BOR send timeout
    std::this_thread::sleep_for(1.5s);
    // Require that transmitter went to error state
    REQUIRE(transmitter.getState() == FSM::State::ERROR);
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
