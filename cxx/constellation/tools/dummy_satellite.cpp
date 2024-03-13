/**
 * @file
 * @brief Dummy satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <memory>
#include <string>

#include <asio.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/logging/SinkManager.hpp"
#include "constellation/satellite/Satellite.hpp"
#include "constellation/satellite/SatelliteImplementation.hpp"

using namespace asio::ip;
using namespace constellation;
using namespace constellation::satellite;

class DummySatellite : public Satellite {
public:
    DummySatellite() = default;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[]) {
    // Get satellite name via cmdline
    std::string satellite_name = host_name();
    if(argc >= 2) {
        satellite_name = argv[1];
    }

    // Create CHIRP manager and set as default
    auto chirp_manager = chirp::Manager(address_v4::broadcast(), address_v4::any(), "cnstln1", satellite_name);
    chirp_manager.setAsDefaultInstance();
    chirp_manager.start();

    // Create and start satellite
    auto satellite = std::make_shared<DummySatellite>();
    auto satellite_implementation = SatelliteImplementation(satellite_name, satellite);
    satellite_implementation.start();

    // Wait for interrupt
    satellite_implementation.join();
}
