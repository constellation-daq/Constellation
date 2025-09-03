/**
 * @file
 * @brief Implementation of EUDAQ data receiver satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "EudaqNativeWriterSatellite.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP2Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;

EudaqNativeWriterSatellite::EudaqNativeWriterSatellite(std::string_view type, std::string_view name)
    : ReceiverSatellite(type, name) {}

void EudaqNativeWriterSatellite::initializing(Configuration& config) {
    base_path_ = config.getPath("output_directory");
    validate_output_directory(base_path_);

    flush_timer_ = TimeoutTimer(std::chrono::seconds(config.get<std::size_t>("flush_interval", 3)));
}

void EudaqNativeWriterSatellite::starting(std::string_view run_identifier) {

    // Fetch sequence from run id:
    const auto pos = run_identifier.find_last_of('_');
    std::uint32_t sequence = 0;
    try {
        sequence = (pos != std::string::npos ? std::stoi(std::string(run_identifier).substr(pos + 1)) : 0);
    } catch(std::invalid_argument&) {
        LOG(DEBUG) << "Could not determine run sequence from run identifier, assuming 0";
    }

    // Open target file
    auto file = create_output_file(base_path_, "data_" + std::string(run_identifier), "raw", true);

    LOG(INFO) << "Starting run with identifier " << run_identifier << ", sequence " << sequence;
    serializer_ = std::make_unique<FileSerializer>(std::move(file), sequence);

    // Start timer for flushing data to file
    flush_timer_.reset();
}

void EudaqNativeWriterSatellite::stopping() {
    serializer_.reset();
}

void EudaqNativeWriterSatellite::interrupting(CSCP::State /*previous_state*/, std::string_view /*reason*/) {
    if(serializer_ != nullptr) {
        serializer_->flush();
    }
    serializer_.reset();
}

void EudaqNativeWriterSatellite::failure(CSCP::State /*previous_state*/, std::string_view /*reason*/) {
    serializer_.reset();
}

void EudaqNativeWriterSatellite::receive_bor(std::string_view sender,
                                             const Dictionary& user_tags,
                                             const Configuration& config) {
    LOG(INFO) << "Received BOR from " << sender << " with config" << config.getDictionary().to_string();
    serializer_->serializeDelimiterMsg(sender, CDTP2Message::Type::BOR, user_tags, config.getDictionary());
}

void EudaqNativeWriterSatellite::receive_data(std::string_view sender, const CDTP2Message::DataBlock& data_block) {
    LOG(DEBUG) << "Received data message from " << sender;
    serializer_->serializeDataBlock(sender, data_block);

    // Flush if necessary and reset timer
    if(flush_timer_.timeoutReached()) {
        serializer_->flush();
        flush_timer_.reset();
    }
}

void EudaqNativeWriterSatellite::receive_eor(std::string_view sender,
                                             const Dictionary& user_tags,
                                             const Dictionary& run_metadata) {
    LOG(INFO) << "Received EOR from " << sender << " with metadata" << run_metadata.to_string();
    serializer_->serializeDelimiterMsg(sender, CDTP2Message::Type::EOR, user_tags, run_metadata);
}
