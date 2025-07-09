/**
 * @file
 * @brief Implementation of the Sputnik prototype satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "SputnikSatellite.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <stop_token>
#include <string_view>
#include <thread>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/metrics/stat.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::metrics;
using namespace constellation::protocol::CSCP;
using namespace constellation::satellite;
using namespace std::chrono_literals;

SputnikSatellite::SputnikSatellite(std::string_view type, std::string_view name) : Satellite(type, name) {
    LOG(STATUS) << "Sputnik prototype satellite " << getCanonicalName() << " created";
    support_reconfigure(true);

    register_command("get_channel_reading",
                     "This example command reads the a device value from the channel number provided as argument. Since this"
                     "will reset the corresponding channel, this can only be done before the run has started.",
                     {State::NEW, State::INIT, State::ORBIT},
                     [](int channel) { return 13.8 * channel; });

    register_timed_metric("BEEP", "beeps", MetricType::LAST_VALUE, "Sputnik beeps", 3s, []() { return 42; });
    register_metric("TIME", "s", MetricType::LAST_VALUE, "Sputnik total time since launch");
    register_metric("TEMPERATURE", "degC", MetricType::LAST_VALUE, "Measured temperature inside satellite");
    register_metric("FAN_RUNNING", "", MetricType::LAST_VALUE, "Information on the fan state");
}

void SputnikSatellite::initializing(Configuration& config) {
    // Obtain the beeping interval from the configuration:
    const auto interval = config.get<std::uint64_t>("interval", 3000U);

    // Obtain launch delay from the configuration:
    launch_delay_ = std::chrono::seconds(config.get<std::uint64_t>("launch_delay", 0));

    register_timed_metric(
        "BEEP", "beeps", MetricType::LAST_VALUE, "Sputnik beeps", std::chrono::milliseconds(interval), []() { return 42; });
}

void SputnikSatellite::reconfiguring(const constellation::config::Configuration& config) {
    if(config.has("interval")) {
        register_timed_metric("BEEP",
                              "beeps",
                              MetricType::LAST_VALUE,
                              "Sputnik beeps",
                              std::chrono::milliseconds(config.get<std::uint64_t>("interval")),
                              []() { return 42; });
    }
}

void SputnikSatellite::launching() {
    // Wait for launch delay
    std::this_thread::sleep_for(launch_delay_);
    // Set launch time
    launch_time_ = std::chrono::system_clock::now();
}

void SputnikSatellite::running(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        const auto time = static_cast<double>((std::chrono::system_clock::now() - launch_time_).count()) * 1e-9;
        // Let's calculate some temperature in space which depends on time (absorption from sun)
        const auto temperature = (std::sin(time / 50.) * 70.) + 20.;

        STAT_T("TEMPERATURE", temperature, 3s);
        // Fan turns on above 36degC
        STAT_T("FAN_RUNNING", temperature > 36., 5s);
        STAT_T("TIME", time, 10s);
        std::this_thread::sleep_for(50ms);
    }
}
