/**
 * @file
 * @brief Implementation of EUDAQ data serializer
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <utility>
#include <vector>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/satellite/exceptions.hpp"

#include "EudaqNativeWriterSatellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::satellite;

EudaqNativeWriterSatellite::FileSerializer::FileSerializer(const std::filesystem::path& path,
                                                           std::uint32_t run_sequence,
                                                           bool overwrite)
    : file_(path, std::ios::binary), run_sequence_(run_sequence) {
    if(std::filesystem::exists(path) && !overwrite) {
        throw SatelliteError("File path exists: " + path.string());
    }

    if(!file_.good()) {
        throw SatelliteError("Error opening file: " + path.string());
    }
}

EudaqNativeWriterSatellite::FileSerializer::~FileSerializer() {
    if(file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void EudaqNativeWriterSatellite::FileSerializer::write(const std::uint8_t* data, std::size_t len) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    file_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
    if(!file_.good()) {
        throw SatelliteError("Error writing to file");
    }
    bytes_written_ += len;
}

void EudaqNativeWriterSatellite::FileSerializer::write_str(const std::string& t) {
    write_int(static_cast<std::uint32_t>(t.length()));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    write(reinterpret_cast<const std::uint8_t*>(t.data()), t.length());
}

void EudaqNativeWriterSatellite::FileSerializer::write_tags(const Dictionary& dict) {
    LOG(DEBUG) << "Writing " << dict.size() << " event tags";

    write_int(static_cast<std::uint32_t>(dict.size()));
    for(const auto& i : dict) {
        write_str(i.first);
        write_str(i.second.str());
    }
}

void EudaqNativeWriterSatellite::FileSerializer::write_blocks(const std::vector<PayloadBuffer>& payload) {
    LOG(DEBUG) << "Writing " << payload.size() << " data blocks";

    // EUDAQ expects a map with frame number as key and vector of uint8_t as value:
    write_int(static_cast<std::uint32_t>(payload.size()));
    for(std::uint32_t key = 0; key < static_cast<std::uint32_t>(payload.size()); key++) {
        write_block(key, payload.at(key));
    }
}

void EudaqNativeWriterSatellite::FileSerializer::write_block(std::uint32_t key, const PayloadBuffer& payload) {
    write_int(key);
    const auto frame = payload.span();
    write_int(static_cast<std::uint32_t>(frame.size_bytes()));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    write(reinterpret_cast<const std::uint8_t*>(frame.data()), frame.size_bytes());
}

void EudaqNativeWriterSatellite::FileSerializer::serialize_header(const constellation::message::CDTP1Message::Header& header,
                                                                  const constellation::config::Dictionary& tags,
                                                                  std::uint32_t flags) {
    LOG(DEBUG) << "Writing event header";

    // Type, version and flags
    write_int(cstr2hash("RawEvent"));
    write_int<std::uint32_t>(0);
    write_int<std::uint32_t>(flags);

    // Number of devices/streams/planes - seems rarely used
    write_int<std::uint32_t>(0);

    // Run sequence
    write_int(run_sequence_);

    // Downcast event sequence for message header, use the same for trigger number
    write_int(static_cast<std::uint32_t>(header.getSequenceNumber()));
    const auto trigger_number_it = tags.find("trigger_number");
    write_int(trigger_number_it != tags.end() ? trigger_number_it->second.get<std::uint32_t>()
                                              : static_cast<std::uint32_t>(header.getSequenceNumber()));

    // Writing ExtendWord (event description, used to identify decoder later on)
    const auto canonical_name = std::string(header.getSender());
    const auto descriptor = eudaq_event_descriptors_.at(canonical_name);
    write_int(cstr2hash(descriptor.c_str()));

    // Timestamps from header tags if available - we get them in ps and write them in ns
    const auto timestamp_begin_it = tags.find("timestamp_begin");
    write_int(timestamp_begin_it != tags.end() ? timestamp_begin_it->second.get<std::uint64_t>() : std::uint64_t());
    const auto timestamp_end_it = tags.find("timestamp_end");
    write_int(timestamp_end_it != tags.end() ? timestamp_end_it->second.get<std::uint64_t>() : std::uint64_t());

    // Event description string
    write_str(descriptor);

    // Header tags
    write_tags(tags);
}

void EudaqNativeWriterSatellite::FileSerializer::parse_bor_tags(const CDTP1Message::Header& header) {
    const auto& tags = header.getTags();
    const auto canonical_name = header.getSender();

    // Check for event type flag:
    const auto eudaq_event_it = tags.find("eudaq_event");
    if(eudaq_event_it != tags.end()) {
        const auto eudaq_event = eudaq_event_it->second.get<std::string>();
        LOG(INFO) << "Using EUDAQ event type " << std::quoted(eudaq_event) << " for sender " << canonical_name;
        eudaq_event_descriptors_.emplace(canonical_name, eudaq_event);
    } else {
        // Take event descriptor tag from sender name:
        const auto separator_pos = canonical_name.find_first_of('.');
        const auto descriptor = canonical_name.substr(separator_pos + 1);
        LOG(WARNING) << "BOR message of " << canonical_name << " does not provide EUDAQ event type - will use sender name "
                     << descriptor << " instead";
        eudaq_event_descriptors_.emplace(canonical_name, descriptor);
    }

    // Check for tag describing treatment of frames:
    const auto frames_as_blocks_it = tags.find("frames_as_blocks");
    if(frames_as_blocks_it != tags.end()) {
        const auto frames_as_blocks = frames_as_blocks_it->second.get<bool>();
        LOG(INFO) << "Sender " << canonical_name << " requests treatment of frames as "
                  << (frames_as_blocks ? "blocks" : "sub-events");
        frames_as_blocks_.emplace(canonical_name, frames_as_blocks);
    } else {
        LOG(WARNING) << "BOR message of " << canonical_name
                     << " does not provide information on frame treatment - defaulting to \"frames as sub-events\"";
        frames_as_blocks_.emplace(canonical_name, false);
    }
}

void EudaqNativeWriterSatellite::FileSerializer::serializeDelimiterMsg(const CDTP1Message::Header& header,
                                                                       const Dictionary& payload) {
    LOG(DEBUG) << "Writing delimiter event";

    // Set correct flags for BORE and EORE:
    std::uint32_t flags = 0;
    if(header.getType() == CDTP1Message::Type::BOR) {
        flags |= std::to_underlying(EUDAQFlags::BORE);
    } else if(header.getType() == CDTP1Message::Type::EOR) {
        flags |= std::to_underlying(EUDAQFlags::EORE);
    }

    // Parse BOR tags to set event descriptor and frame handling
    if(header.getType() == CDTP1Message::Type::BOR) {
        parse_bor_tags(header);
    }

    // Serialize header with event flags
    serialize_header(header, payload, flags);

    // BOR does not contain data - write empty blocks and empty subevent count:
    write_blocks({});
    write_int<std::uint32_t>(0);
}

void EudaqNativeWriterSatellite::FileSerializer::serializeDataMsg(const CDTP1Message& data_message) {

    LOG(DEBUG) << "Writing data event";

    const auto& header = data_message.getHeader();
    serialize_header(header, header.getTags());

    auto canonical_name = std::string(header.getSender());
    if(frames_as_blocks_.at(canonical_name)) {
        // Interpret multiple frames as individual blocks of EUDAQ data:

        // Write block data:
        write_blocks(data_message.getPayload());

        // Zero sub-events:
        write_int<std::uint32_t>(0);
    } else {
        // Interpret each payload frame as a EUDAQ sub-event:

        // Write zero blocks:
        write_int<std::uint32_t>(0);

        // Write subevents:
        const auto& payload = data_message.getPayload();
        write_int(static_cast<std::uint32_t>(payload.size()));

        for(const auto& frame : payload) {
            // Repeat the event header of this event - FIXME adjust event number!
            serialize_header(header, header.getTags());

            // Write number of blocks and the block itself
            write_int<std::uint32_t>(1);
            write_block(0, frame);
        }
    }
}