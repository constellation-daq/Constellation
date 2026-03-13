/**
 * @file
 * @brief Functions for loading C++ satellites
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/networking/asio_helpers.hpp"
#include "constellation/exec/DSOLoader.hpp"
#include "constellation/exec/satellite.hpp"
#include "constellation/satellite/Satellite.hpp"

namespace constellation::exec {
    class CNSTLN_API LoadedCppSatellite final : public LoadedSatellite {
    public:
        /**
         * @brief Create a loaded C++ satellite
         *
         * @param satellite_type Satellite type to load
         */
        LoadedCppSatellite(const SatelliteType& satellite_type);

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
        /** Loader holding the Dynamic Shared Object (DSO) */
        DSOLoader loader_;

        /** Generator function for the satellite */
        satellite::Generator* satellite_generator_;

        /** Satellite instance */
        std::shared_ptr<satellite::Satellite> satellite_;
    };

} // namespace constellation::exec
