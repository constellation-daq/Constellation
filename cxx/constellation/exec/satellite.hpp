/**
 * @file
 * @brief Main function for a satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/networking/asio_helpers.hpp"

namespace constellation::exec {

    struct SatelliteType {
        SatelliteType(std::string _type_name, std::filesystem::path _dso_path = {})
            : type_name(std::move(_type_name)), dso_path(std::move(_dso_path)) {}

        /** Name of satellite type */
        std::string type_name;

        /** Path to the Dynamic Shared Object (DSO) that contains the satellite */
        std::filesystem::path dso_path;
    };

    class CNSTLN_API LoadedSatellite {
    public:
        LoadedSatellite() = default;
        virtual ~LoadedSatellite() = default;

        /// @cond doxygen_suppress
        // No copy/move constructor/assignment
        LoadedSatellite(const LoadedSatellite& other) = delete;
        LoadedSatellite& operator=(const LoadedSatellite& other) = delete;
        LoadedSatellite(LoadedSatellite&& other) = delete;
        LoadedSatellite& operator=(LoadedSatellite&& other) = delete;
        /// @endcond

        /**
         * @brief Start the satellite
         *
         * @param group Group to start the satellite in
         * @param satellite_name Name of the satellite
         * @param log_level Log level of the satellite
         * @param interfaces Network interfaces to use
         */
        virtual void start(std::string_view group,
                           std::string_view satellite_name,
                           log::Level log_level,
                           const std::vector<networking::Interface>& interfaces) = 0;

        /**
         * @brief Join the satellite
         */
        virtual void join() = 0;

        /**
         * @brief Get type name as loaded from the library
         *
         * @return Type name of the loaded satellite
         */
        std::string_view getTypeName() const { return type_name_; }

    protected:
        void set_type_name(std::string type_name) { type_name_ = std::move(type_name); }

    private:
        std::string type_name_;
    };

    /**
     * Provides the main function for a satellite
     *
     * @param args CLI arguments
     * @param program Name of the CLI executable
     * @param satellite_type Optional satellite type to pre-load
     */
    CNSTLN_API int satellite_main(std::span<const char*> args,
                                  std::string_view program,
                                  std::optional<SatelliteType> satellite_type = std::nullopt) noexcept;

    // Handler for signal like SIGINT etc
    extern "C" CNSTLN_API void signal_hander(int signal);

} // namespace constellation::exec
