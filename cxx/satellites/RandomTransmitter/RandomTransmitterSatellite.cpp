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
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

using namespace constellation::config;
using namespace constellation::metrics;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;

RandomTransmitterSatellite::RandomTransmitterSatellite(std::string_view type, std::string_view name)
    : TransmitterSatellite(type, name), byte_rng_(generate_random_seed()) {
    support_reconfigure();

    register_timed_metric("RATE_LIMITED",
                          "",
                          MetricType::LAST_VALUE,
                          "Counts how often data sending was skipped due the data rate limitations",
                          5s,
                          [this]() { return rate_limited_.load(); });
}

std::uint32_t RandomTransmitterSatellite::generate_random_seed() {
    std::random_device rng {};
    return static_cast<std::uint32_t>(rng());
}

void RandomTransmitterSatellite::initializing(Configuration& config) {
    pregen_ = config.get<bool>("pregen", false);
    seed_ = config.get<std::uint32_t>("seed", generate_random_seed());
    frame_size_ = config.get<std::uint64_t>("frame_size", 1024U);
    number_of_frames_ = config.get<std::uint32_t>("number_of_frames", 1U);
    LOG(STATUS) << "Initialized with seed " << to_string(seed_) << " and " << frame_size_
                << " bytes per data frame, sending " << number_of_frames_ << " "
                << (number_of_frames_ == 1 ? "frame" : "frames") << " per message"
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
    if(partial_config.has("frame_size")) {
        frame_size_ = partial_config.get<std::uint64_t>("frame_size");
        LOG(STATUS) << "Reconfigured frame size: " << frame_size_;
    }
    if(partial_config.has("number_of_frames")) {
        number_of_frames_ = partial_config.get<std::uint32_t>("number_of_frames");
        LOG(STATUS) << "Reconfigured number of frames: " << number_of_frames_;
    }
}

void RandomTransmitterSatellite::starting(std::string_view run_identifier) {
    std::seed_seq seed_seq {seed_};
    byte_rng_.seed(seed_seq);
    rate_limited_ = 0;
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
        // Skip sending if data rate limited
        if(checkDataRateLimited()) {
            ++rate_limited_;
            std::this_thread::sleep_for(1ms);
            continue;
        }
        // Create new data block
        auto data_block = newDataBlock(number_of_frames_);
        for(std::uint32_t n = 0; n < number_of_frames_; ++n) {
            // Generate random bytes
            std::vector<std::uint8_t> data {};
            data.resize(frame_size_);
            std::ranges::generate(data, std::ref(byte_rng_));
            // Add data to data block
            data_block.addFrame(std::move(data));
        }
        sendDataBlock(std::move(data_block));
    }
}

void RandomTransmitterSatellite::running_pregen(const std::stop_token& stop_token) {
    // Pre-generate random data
    std::vector<std::vector<std::uint8_t>> frames {};
    frames.reserve(number_of_frames_);
    for(std::uint32_t n = 0; n < number_of_frames_; ++n) {
        // Generate random bytes
        std::vector<std::uint8_t> data {};
        data.resize(frame_size_);
        std::ranges::generate(data, std::ref(byte_rng_));
        // Store data for later
        frames.emplace_back(std::move(data));
    }
    LOG(INFO) << "Generation of random data complete";
    // Actual sending loop
    while(!stop_token.stop_requested()) {
        // Skip sending if data rate limited
        if(checkDataRateLimited()) {
            ++rate_limited_;
            std::this_thread::sleep_for(1ms);
            continue;
        }
        // Create data
        auto data_block = newDataBlock(frames.size());
        for(const auto& frame : frames) {
            // Copy vector to frame
            data_block.addFrame({std::vector(frame)});
        }
        sendDataBlock(std::move(data_block));
    }
}

void RandomTransmitterSatellite::stopping() {
    LOG_IF(WARNING, rate_limited_ > 0) << "Reached data rate limit " << rate_limited_ << " times";
}
