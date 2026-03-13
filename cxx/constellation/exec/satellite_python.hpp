/**
 * @file
 * @brief Functions for loading Python satellites
 *
 * @copyright Copyright (c) 2026 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/networking/asio_helpers.hpp"
#include "constellation/exec/PyLoader.hpp"
#include "constellation/exec/satellite.hpp"

namespace constellation::exec {

    class CNSTLN_API LoadedPythonSatellite final : public LoadedSatellite {
    public:
        /**
         * @brief Create a loaded Python satellite
         *
         * @param satellite_type Satellite type to load
         */
        LoadedPythonSatellite(const SatelliteType& satellite_type);

        /**
         * @brief Start the satellite
         *
         * @param group Group to start the satellite in
         * @param satellite_name Name of the satellite
         * @param log_level Log level of the satellite
         * @param interfaces Network interfaces to use
         */
        void start(std::string_view group,
                   std::string_view satellite_name,
                   log::Level log_level,
                   const std::vector<networking::Interface>& interfaces) final;

        /**
         * @brief Join the satellite
         */
        void join() final;

    private:
        PyLoader py_loader_;
        std::string module_name_;
    };

} // namespace constellation::exec
