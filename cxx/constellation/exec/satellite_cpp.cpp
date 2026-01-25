/**
 * @file
 * @brief Implementation of functions for loading C++ satellites
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "satellite_cpp.hpp"

#include <chrono> // IWYU pragma: keep
#include <csignal>
#include <string_view>
#include <thread>
#include <vector>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/networking/asio_helpers.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/exec/cli.hpp"
#include "constellation/exec/satellite.hpp"

using namespace constellation;
using namespace constellation::exec;
using namespace constellation::log;
using namespace constellation::networking;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;

// Global variable for signal handler
namespace {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    volatile std::sig_atomic_t signal_v {0};
} // namespace

// The only safe thing a signal handler can do is setting an atomic int
extern "C" void signal_handler(int signal) {
    signal_v = signal;
}

LoadedCppSatellite::LoadedCppSatellite(const SatelliteType& satellite_type)
    : loader_(satellite_type.type_name, satellite_type.dso_path), satellite_generator_(loader_.loadSatelliteGenerator()) {
    set_type_name(loader_.getDSOName());
}

void LoadedCppSatellite::start(std::string_view group,
                               std::string_view satellite_name,
                               Level /*log_level*/,
                               const std::vector<networking::Interface>& interfaces) {
    const auto canonical_name = to_string(getTypeName()) + " " + to_string(satellite_name);
    constellation_setup_chirp(group, canonical_name, interfaces);
    satellite_ = satellite_generator_(getTypeName(), satellite_name);
}

void LoadedCppSatellite::join() {
    // Register signal handler
    std::signal(SIGTERM, &signal_handler); // NOLINT(cert-err33-c)
    std::signal(SIGINT, &signal_handler);  // NOLINT(cert-err33-c)

    // Wait for signal or satellite termination
    while(signal_v == 0 && !satellite_->terminated()) {
        std::this_thread::sleep_for(50ms);
    }

    // Terminate satellite if not terminated already
    if(!satellite_->terminated()) {
        LOG(STATUS) << "Terminating satellite";
        satellite_->terminate();
    }

    // Join satellite
    satellite_->join();

    // Unregister callbacks
    ManagerLocator::getCHIRPManager()->unregisterDiscoverCallbacks();
}
