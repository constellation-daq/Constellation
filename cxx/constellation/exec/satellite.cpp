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
#include "constellation/core/utils/string.hpp"
#include "constellation/exec/cli.hpp"
#include "constellation/exec/cpp.hpp"
#include "constellation/exec/exceptions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::exec;
using namespace constellation::satellite;
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

        // Load satellite DSO
        LoadedCppSatellite loaded_satellite {};
        try {
            loaded_satellite = load_cpp_satellite(satellite_type_v);
        } catch(const DSOLoaderError& error) {
            LOG(CRITICAL) << "Error loading satellite type " + quote(satellite_type_v.type_name) + ": " + error.what();
            return 1;
        }

        // Get canonical name
        const auto canonical_name = loaded_satellite.type_name + "." + options.satellite_name;

        // Setup CHIRP
        constellation_setup_chirp(options.group, canonical_name, options.interfaces);

        // Create satellite
        LOG(STATUS) << "Starting satellite " << canonical_name;
        std::shared_ptr<Satellite> satellite {};
        try {
            satellite = loaded_satellite.satellite_generator(satellite_type_v.type_name, options.satellite_name);
        } catch(const std::exception& error) {
            LOG(CRITICAL) << "Failed to create satellite: " << error.what();
            return 1;
        }

        // Join satellite
        join_cpp_satellite(satellite.get());

        return 0;

    } catch(const std::exception& error) {
        std::cerr << "Critical failure: " << error.what() << "\n" << std::flush;
    } catch(...) {
        std::cerr << "Critical failure: <unknown exception>\n" << std::flush;
    }
    return 1;
}
