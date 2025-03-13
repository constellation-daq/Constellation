/**
 * @file
 * @brief Flight Recorder satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string_view>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/listener/LogListener.hpp"
#include "constellation/satellite/Satellite.hpp"

class FlightRecorderSatellite final : public constellation::satellite::Satellite,
                                      public constellation::listener::LogListener {
public:
    FlightRecorderSatellite(std::string_view type, std::string_view name);

    void initializing(constellation::config::Configuration& config) final;

private:
    void add_message(constellation::message::CMDP1LogMessage&& msg);

private:
    std::shared_ptr<spdlog::logger> file_logger_;
};
