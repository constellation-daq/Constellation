/**
 * @file
 * @brief Implementation of random data transmitting satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "RandomTransmitterSatellite.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <random>
#include <stop_token>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/metrics/stat.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

using namespace constellation::config;
using namespace constellation::metrics;
using namespace constellation::protocol::CSCP;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;

RandomTransmitterSatellite::RandomTransmitterSatellite(std::string_view type, std::string_view name)
    : TransmitterSatellite(type, name), byte_rng_(generate_random_seed()) {
    support_reconfigure();

    register_timed_metric(
        "DUTY_CYCLE", "", MetricType::LAST_VALUE, "Total duty cycle of the run loop", 5s, {State::RUN}, [this]() {
            return 1. - (static_cast<double>(rate_limited_.load()) / static_cast<double>(loop_iterations_.load()));
        });
}

std::uint32_t RandomTransmitterSatellite::generate_random_seed() {
    std::random_device rng {};
    return static_cast<std::uint32_t>(rng());
}

void RandomTransmitterSatellite::initializing(Configuration& config) {
    pregen_ = config.get<bool>("pregen", false);
    seed_ = config.get<std::uint32_t>("seed", generate_random_seed());
    block_size_ = config.get<std::uint64_t>("block_size", 1024U);
    number_of_blocks_ = config.get<std::uint32_t>("number_of_blocks", 1U);
    LOG(STATUS) << "Initialized with seed " << to_string(seed_) << " and " << block_size_ << " bytes per block, sending "
                << number_of_blocks_ << " block" << (number_of_blocks_ == 1 ? "" : "s") << " per message"
                << " with " << (pregen_ ? "pre" : "rng") << "-generated data";
}

void RandomTransmitterSatellite::reconfiguring(const Configuration& partial_config) {
    if(partial_config.has("pregen")) {
        pregen_ = partial_config.get<bool>("pregen");
        LOG(STATUS) << "Reconfigured to using " << (pregen_ ? "pre" : "rng") << "-generated data";
    }
    if(partial_config.has("seed")) {
        seed_ = partial_config.get<std::uint32_t>("seed");
        LOG(STATUS) << "Reconfigured seed: " << to_string(seed_);
    }
    if(partial_config.has("block_size")) {
        block_size_ = partial_config.get<std::uint64_t>("block_size");
        LOG(STATUS) << "Reconfigured block size: " << block_size_;
    }
    if(partial_config.has("number_of_blocks")) {
        number_of_blocks_ = partial_config.get<std::uint32_t>("number_of_blocks");
        LOG(STATUS) << "Reconfigured number of blocks: " << number_of_blocks_;
    }
}

void RandomTransmitterSatellite::starting(std::string_view run_identifier) {
    std::seed_seq seed_seq {seed_};
    byte_rng_.seed(seed_seq);
    rate_limited_ = 0;
    loop_iterations_ = 0;
    LOG(INFO) << "Starting run " << run_identifier << " with seed " << to_string(seed_);
}

void RandomTransmitterSatellite::running(const std::stop_token& stop_token) {
    if(pregen_) {
        running_pregen(stop_token);
    } else {
        running_rnggen(stop_token);
    }
}

void RandomTransmitterSatellite::running_rnggen(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        ++loop_iterations_;
        // Skip sending if data rate limited
        if(!canSendRecord()) {
            ++rate_limited_;
            std::this_thread::sleep_for(1ms);
            continue;
        }
        // Create new data record
        auto data_record = newDataRecord(number_of_blocks_);
        for(std::uint32_t n = 0; n < number_of_blocks_; ++n) {
            // Generate random bytes
            std::vector<std::uint8_t> data {};
            data.resize(block_size_);
            std::ranges::generate(data, std::ref(byte_rng_));
            // Add data to data record
            data_record.addBlock(std::move(data));
        }
        sendDataRecord(std::move(data_record));
    }
}

void RandomTransmitterSatellite::running_pregen(const std::stop_token& stop_token) {
    // Pre-generate random data
    std::vector<std::vector<std::uint8_t>> blocks {};
    blocks.reserve(number_of_blocks_);
    for(std::uint32_t n = 0; n < number_of_blocks_; ++n) {
        // Generate random bytes
        std::vector<std::uint8_t> data {};
        data.resize(block_size_);
        std::ranges::generate(data, std::ref(byte_rng_));
        // Store data for later
        blocks.emplace_back(std::move(data));
    }
    LOG(INFO) << "Generation of random data complete";
    // Actual sending loop
    while(!stop_token.stop_requested()) {
        ++loop_iterations_;
        // Skip sending if data rate limited
        if(!canSendRecord()) {
            ++rate_limited_;
            std::this_thread::sleep_for(1ms);
            continue;
        }
        // Create data
        auto data_record = newDataRecord(number_of_blocks_);
        for(const auto& block : blocks) {
            // Copy vector to block
            data_record.addBlock({std::vector(block)});
        }
        sendDataRecord(std::move(data_record));
    }
}

void RandomTransmitterSatellite::stopping() {
    const auto rate_limited = rate_limited_.load();
    const auto loop_iterations = loop_iterations_.load();
    const auto duty_cycle = 1. - (static_cast<double>(rate_limited) / static_cast<double>(loop_iterations));
    STAT("DUTY_CYCLE", duty_cycle);
    LOG_IF(WARNING, rate_limited > 0) << "Reached data rate limit " << rate_limited << " times out of " << loop_iterations
                                      << " loop iterations, leading to a duty cycle of " << duty_cycle;
}
