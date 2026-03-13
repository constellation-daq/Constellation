/**
 * @file
 * @brief Implementation of functions for loading Python satellites
 *
 * @copyright Copyright (c) 2026 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "satellite_python.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/networking/asio_helpers.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/exec/exceptions.hpp"
#include "constellation/exec/satellite.hpp"

using namespace constellation::exec;
using namespace constellation::log;
using namespace constellation::utils;

LoadedPythonSatellite::LoadedPythonSatellite(const SatelliteType& satellite_type) {
    // Load all modules
    const auto modules = py_loader_.loadModules();

    // Check if satellite type with corresponding module exists
    const auto module_it = std::ranges::find(modules,
                                             transform(satellite_type.type_name, ::tolower),
                                             [](const auto& element) { return transform(element.first, ::tolower); });
    if(module_it == modules.cend()) {
        throw PyLoadingError(satellite_type.type_name, "no corresponding Python module found");
    }

    // Store extracted type and module
    set_type_name(module_it->first);
    module_name_ = module_it->second;
}

void LoadedPythonSatellite::start(std::string_view group,
                                  std::string_view satellite_name,
                                  Level log_level,
                                  const std::vector<networking::Interface>& interfaces) {
    // Recreate cli arguments
    std::vector<std::string> args {};
    args.emplace_back("-g");
    args.emplace_back(group);
    args.emplace_back("-n");
    args.emplace_back(satellite_name);
    args.emplace_back("-l");
    args.emplace_back(enum_name(log_level));
    for(const auto& interface : interfaces) {
        args.emplace_back("-i");
        args.emplace_back(interface.name);
    }

    // Run stored module
    py_loader_.runModule(module_name_, args);
}

void LoadedPythonSatellite::join() {
    // Nothing to do
}
