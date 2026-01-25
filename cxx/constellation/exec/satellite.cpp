/**
 * @file
 * @brief Implementation of the main function for a satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "satellite.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <asio.hpp>

#include "constellation/core/log/log.hpp"
#include "constellation/core/utils/exceptions.hpp"
#include "constellation/exec/cli.hpp"
#include "constellation/exec/satellite_cpp.hpp"
#include "constellation/exec/satellite_python.hpp"

using namespace constellation::exec;
using namespace constellation::utils;

int constellation::exec::satellite_main(std::span<const char*> args,
                                        std::string_view program,
                                        std::optional<SatelliteType> satellite_type) noexcept {
    try {
        // Get parser and setup
        auto parser =
            SatelliteParser(std::string(program),
                            satellite_type.has_value() ? std::optional(satellite_type.value().type_name) : std::nullopt);
        parser.setup();

        // Parse options
        SatelliteParser::SatelliteOptions options {};
        try {
            options = parser.parse(args);
        } catch(const std::exception& error) {
            LOG(CRITICAL) << "Argument parsing failed: " << error.what() << "\n\n" << parser.help();
            return 1;
        }

        // Set satellite type if required
        satellite_type = satellite_type.value_or(SatelliteType(options.satellite_type));
        const auto& satellite_type_v = satellite_type.value();

        // Set log level and default topic
        constellation_setup_logging(options.log_level, satellite_type_v.type_name);

        // Load satellite
        std::unique_ptr<LoadedSatellite> loaded_satellite {};
        std::string cpp_load_exception {};
        std::string py_load_exception {};

        // Try loading C++ satellite
        try {
            loaded_satellite = std::make_unique<LoadedCppSatellite>(satellite_type_v);
        } catch(const RuntimeError& error) {
            LOG(TRACE) << "Loading C++ satellite failed: " << error.what();
            cpp_load_exception = error.what();
        }

        // Try loading Python satellite
        try {
            loaded_satellite = std::make_unique<LoadedPythonSatellite>(satellite_type_v);
        } catch(const RuntimeError& error) {
            LOG(TRACE) << "Loading Python satellite failed: " << error.what();
            py_load_exception = error.what();
        }

        if(!loaded_satellite) {
            LOG(CRITICAL) << "Could not load satellite of type " << satellite_type_v.type_name;
            return 1;
        }

        // Start satellite
        LOG(STATUS) << "Starting satellite " << loaded_satellite->getTypeName() << "." << options.satellite_name;
        loaded_satellite->start(options.group, options.satellite_name, options.log_level, options.interfaces);

        // Join satellite
        loaded_satellite->join();

        return 0;

    } catch(const std::exception& error) {
        std::cerr << "Critical failure: " << error.what() << "\n" << std::flush;
    } catch(...) {
        std::cerr << "Critical failure: <unknown exception>\n" << std::flush;
    }
    return 1;
}
