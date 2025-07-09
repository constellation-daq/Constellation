/**
 * @file
 * @brief Implementation of functions to build executables interfacing with C++
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "cpp.hpp"

#include <chrono> // IWYU pragma: keep
#include <csignal>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/networking/asio_helpers.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/exec/DSOLoader.hpp"
#include "constellation/exec/satellite.hpp"
#include "constellation/satellite/Satellite.hpp"

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

void constellation::exec::constellation_setup_logging(Level default_level, std::string_view default_topic) {
    // Set default log level
    ManagerLocator::getSinkManager().setConsoleLevels(default_level);

    // Set default topic
    ManagerLocator::getSinkManager().setDefaultTopic(default_topic);

    // Log version
    LOG(STATUS) << "Constellation " << CNSTLN_VERSION_FULL;
}

void constellation::exec::constellation_setup_chirp(std::string_view group,
                                                    std::string_view name,
                                                    const std::vector<Interface>& interfaces) {
    // Create CHIRP manager and set as default
    std::unique_ptr<chirp::Manager> chirp_manager {};
    chirp_manager = std::make_unique<chirp::Manager>(group, name, interfaces);
    chirp_manager->start();
    ManagerLocator::setDefaultCHIRPManager(std::move(chirp_manager));

    // Register CMDP in CHIRP and set sender name for CMDP
    ManagerLocator::getSinkManager().enableCMDPSending(std::string(name));
}

LoadedCppSatellite constellation::exec::load_cpp_satellite(const SatelliteType& satellite_type) {
    std::unique_ptr<DSOLoader> loader {};
    Generator* satellite_generator {};
    loader = std::make_unique<DSOLoader>(satellite_type.type_name, satellite_type.dso_path);
    satellite_generator = loader->loadSatelliteGenerator();
    return {satellite_generator, loader->getDSOName(), std::move(loader)};
}

void constellation::exec::join_cpp_satellite(Satellite* satellite) {
    // Register signal handler
    std::signal(SIGTERM, &signal_handler); // NOLINT(cert-err33-c)
    std::signal(SIGINT, &signal_handler);  // NOLINT(cert-err33-c)

    // Wait for signal or satellite termination
    while(signal_v == 0 && !satellite->terminated()) {
        std::this_thread::sleep_for(50ms);
    }

    // Terminate satellite if not terminated already
    if(!satellite->terminated()) {
        LOG(STATUS) << "Terminating satellite";
        satellite->terminate();
    }

    // Join satellite
    satellite->join();

    // Unregister callbacks
    ManagerLocator::getCHIRPManager()->unregisterDiscoverCallbacks();
}
