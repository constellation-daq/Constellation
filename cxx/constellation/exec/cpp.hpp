/**
 * @file
 * @brief Functions to build executables interfacing with C++
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/networking/asio_helpers.hpp"
#include "constellation/exec/DSOLoader.hpp"
#include "constellation/exec/satellite.hpp"
#include "constellation/satellite/Satellite.hpp"

namespace constellation::exec {

    /**
     * @brief Setup Logging
     *
     * @param default_level Default log level for the console output
     * @param default_topic Topic for default logger (type part the canonical name)
     */
    CNSTLN_API void constellation_setup_logging(log::Level default_level, std::string_view default_topic);

    /**
     * @brief Setup CHIRP
     *
     * @param group Constellation group name
     * @param name CHIRP hostname
     * @param interfaces List of network interfaces to use
     */
    CNSTLN_API void constellation_setup_chirp(std::string_view group,
                                              std::string_view name,
                                              const std::vector<networking::Interface>& interfaces);

    struct LoadedCppSatellite {
        /** Generator function for the satellite */
        satellite::Generator* satellite_generator;

        /** Properly capitalized satellite type */
        std::string type_name;

        /** Loader holding the Dynamic Shared Object (DSO) */
        std::unique_ptr<DSOLoader> loader;
    };

    /**
     * @brief Load a satellite
     *
     * @param satellite_type Satellite type to load
     * @return Loaded satellite which can be used to generate a satellite
     */
    CNSTLN_API LoadedCppSatellite load_cpp_satellite(const SatelliteType& satellite_type);

    /**
     * @brief Join a satellite
     *
     * Registers signal handlers to terminate satellite and waits until main satellite thread is joined
     *
     * @param satellite Pointer to satellite
     */
    CNSTLN_API void join_cpp_satellite(satellite::Satellite* satellite);

} // namespace constellation::exec
