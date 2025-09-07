/**
 * @file
 * @brief Random data transmitting satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <random>
#include <stop_token>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

class RandomTransmitterSatellite final : public constellation::satellite::TransmitterSatellite {
public:
    RandomTransmitterSatellite(std::string_view type, std::string_view name);

    void initializing(constellation::config::Configuration& config) final;
    void reconfiguring(const constellation::config::Configuration& partial_config) final;
    void starting(std::string_view run_identifier) final;
    void running(const std::stop_token& stop_token) final;
    void stopping() final;

private:
    static std::uint32_t generate_random_seed();
    void running_rnggen(const std::stop_token& stop_token);
    void running_pregen(const std::stop_token& stop_token);

private:
    bool pregen_ {};
    std::uint32_t seed_ {};
    std::independent_bits_engine<std::default_random_engine, std::numeric_limits<std::uint8_t>::digits, std::uint8_t>
        byte_rng_;
    std::uint64_t block_size_ {};
    std::uint32_t number_of_blocks_ {};
    std::atomic_size_t rate_limited_;
    std::atomic_size_t loop_iterations_;
};
