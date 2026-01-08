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
#include "constellation/core/config/value_types.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP2Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"

#include "FileSerializer.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;

EudaqNativeWriterSatellite::EudaqNativeWriterSatellite(std::string_view type, std::string_view name)
    : ReceiverSatellite(type, name), buffer_size_(), flush_timer_({}) {}

void EudaqNativeWriterSatellite::initializing(Configuration& config) {
    base_path_ = config.getPath("output_directory");
    validate_output_directory(base_path_);

    buffer_size_ = config.get<std::size_t>("buffer_size", 128) * 1024;
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
    serializer_ = std::make_unique<FileSerializer>(std::move(file), buffer_size_, sequence);

    // Start timer for flushing data to file
    flush_timer_.reset();
}

void EudaqNativeWriterSatellite::stopping() {
    serializer_.reset();
}

void EudaqNativeWriterSatellite::failure(CSCP::State /*previous_state*/, std::string_view /*reason*/) {
    serializer_.reset();
}

void EudaqNativeWriterSatellite::receive_bor(std::string_view sender,
                                             const Dictionary& user_tags,
                                             const Dictionary& config) {
    // Add the configuration as single key to the BOR tags:
    auto header_tags = user_tags;
    header_tags["EUDAQ_CONFIG"] = config.to_string();

    serializer_->serializeDelimiterMsg(sender, CDTP2Message::Type::BOR, header_tags);
}

void EudaqNativeWriterSatellite::receive_data(std::string_view sender, const CDTP2Message::DataRecord& data_record) {
    serializer_->serializeDataRecord(sender, data_record);

    // Flush if necessary and reset timer
    if(flush_timer_.timeoutReached()) {
        serializer_->flush();
        flush_timer_.reset();
    }
}

void EudaqNativeWriterSatellite::receive_eor(std::string_view sender,
                                             const Dictionary& user_tags,
                                             const Dictionary& run_metadata) {
    // Merge user tags and metadata:
    auto merged_tags = user_tags;
    merged_tags.insert(run_metadata.begin(), run_metadata.end());

    serializer_->serializeDelimiterMsg(sender, CDTP2Message::Type::EOR, merged_tags);
}
