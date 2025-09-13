/**
 * @file
 * @brief Implementation of EUDAQ data serializer
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "FileSerializer.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP2Message.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/casts.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/exceptions.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::satellite;
using namespace constellation::utils;

FileSerializer::FileSerializer(std::ofstream file, std::uint32_t run_sequence)
    : file_(std::move(file)), run_sequence_(run_sequence) {}

FileSerializer::~FileSerializer() {
    if(file_.is_open()) {
        file_.close();
    }
}

void FileSerializer::flush() {
    if(file_.is_open()) {
        file_.flush();
    }
}

void FileSerializer::write(std::span<const std::byte> data) {
    file_.write(to_char_ptr(data.data()), static_cast<std::streamsize>(data.size_bytes()));
    if(!file_.good()) {
        throw SatelliteError("Error writing to file");
    }
}

void FileSerializer::write_str(std::string_view t) {
    write_int(static_cast<std::uint32_t>(t.length()));
    write({to_byte_ptr(t.data()), t.length()}); // NOLINT(bugprone-suspicious-stringview-data-usage)
}

void FileSerializer::write_tags(const Dictionary& dict) {
    LOG(DEBUG) << "Writing " << dict.size() << " event tags";

    write_int(static_cast<std::uint32_t>(dict.size()));
    for(const auto& i : dict) {
        write_str(i.first);
        write_str(i.second.str());
    }
}

void FileSerializer::write_blocks(const std::vector<PayloadBuffer>& payload) {
    LOG(DEBUG) << "Writing " << payload.size() << " data records";

    // EUDAQ expects a map with block number as key and vector of uint8_t as value:
    write_int(static_cast<std::uint32_t>(payload.size()));
    for(std::uint32_t key = 0; key < static_cast<std::uint32_t>(payload.size()); key++) {
        write_block(key, payload.at(key));
    }
}

void FileSerializer::write_block(std::uint32_t key, const PayloadBuffer& payload) {
    write_int(key);
    const auto data = payload.span();
    write_int(static_cast<std::uint32_t>(data.size_bytes()));
    write(data);
}

void FileSerializer::serialize_header(std::string_view sender_lc,
                                      std::uint64_t sequence_number,
                                      const constellation::config::Dictionary& tags,
                                      std::uint32_t flags) {
    LOG(DEBUG) << "Writing event header";

    // If we have a trigger flag set, also add the corresponding EUDAQ flag:
    const auto trigger_flag_it = tags.find("flag_trigger");
    if(trigger_flag_it != tags.end()) {
        flags |= trigger_flag_it->second.get<bool>() ? std::to_underlying(EUDAQFlags::TRIGGER) : 0;
    }

    // Type, version and flags
    write_int(cstr2hash("RawEvent"));
    write_int<std::uint32_t>(0);
    write_int<std::uint32_t>(flags);

    // Number of devices/streams/planes - seems rarely used
    write_int<std::uint32_t>(0);

    // Run sequence
    write_int(run_sequence_);

    // Downcast event sequence for message header, use the same for trigger number
    write_int(static_cast<std::uint32_t>(sequence_number));
    const auto trigger_number_it = tags.find("trigger_number");
    write_int(trigger_number_it != tags.end() ? trigger_number_it->second.get<std::uint32_t>()
                                              : static_cast<std::uint32_t>(sequence_number));

    // Writing ExtendWord (event description, used to identify decoder later on)
    const auto& descriptor = eudaq_event_descriptors_.find(sender_lc)->second;
    write_int(cstr2hash(descriptor.c_str()));

    // Timestamps from header tags if available - we get them in ps form the Constellation header tags and write them in ns
    // to the EUDAQ event
    const auto timestamp_begin_it = tags.find("timestamp_begin");
    write_int(timestamp_begin_it != tags.end() ? timestamp_begin_it->second.get<std::uint64_t>() / 1000 : std::uint64_t());
    const auto timestamp_end_it = tags.find("timestamp_end");
    write_int(timestamp_end_it != tags.end() ? timestamp_end_it->second.get<std::uint64_t>() / 1000 : std::uint64_t());

    // Event description string
    write_str(descriptor);

    // Header tags
    write_tags(tags);
}

void FileSerializer::parse_bor_tags(std::string_view sender, const Dictionary& user_tags) {
    const auto sender_lc = transform(sender, ::tolower);

    // Check for event type flag:
    const auto eudaq_event_it = user_tags.find("eudaq_event");
    if(eudaq_event_it != user_tags.end()) {
        const auto eudaq_event = eudaq_event_it->second.get<std::string>();
        LOG(INFO) << "Using EUDAQ event type " << quote(eudaq_event) << " for sender " << sender;
        eudaq_event_descriptors_.emplace(sender_lc, eudaq_event);
    } else {
        // Take event descriptor tag from sender name:
        const auto separator_pos = sender.find_first_of('.');
        const auto descriptor = sender.substr(separator_pos + 1);
        LOG(WARNING) << "BOR message of " << sender << " does not provide EUDAQ event type - will use sender name "
                     << descriptor << " instead";
        eudaq_event_descriptors_.emplace(sender_lc, descriptor);
    }

    // Check for tag describing treatment of blocks
    const auto write_as_blocks_it = user_tags.find("write_as_blocks");
    if(write_as_blocks_it != user_tags.end()) {
        const auto write_as_blocks = write_as_blocks_it->second.get<bool>();
        LOG(INFO) << "Sender " << sender << " requests treatment of blocks as "
                  << (write_as_blocks ? "blocks" : "sub-events");
        write_as_blocks_.emplace(sender_lc, write_as_blocks);
    } else {
        LOG(WARNING) << "BOR message of " << sender << " does not provide information on block treatment - defaulting to "
                     << "blocks as sub-events"_quote;
        write_as_blocks_.emplace(sender_lc, false);
    }
}

void FileSerializer::serializeDelimiterMsg(std::string_view sender, CDTP2Message::Type type, const Dictionary& tags) {
    LOG(DEBUG) << "Writing delimiter event for " << sender;
    const auto sender_lc = transform(sender, ::tolower);

    // Set correct flags for BORE and EORE:
    std::uint32_t flags = 0;
    switch(type) {
    case CDTP2Message::Type::BOR: flags |= std::to_underlying(EUDAQFlags::BORE); break;
    case CDTP2Message::Type::EOR: flags |= std::to_underlying(EUDAQFlags::EORE); break;
    default: std::unreachable();
    }

    // Parse BOR tags to set event descriptor and block handling
    if(type == CDTP2Message::Type::BOR) {
        parse_bor_tags(sender, tags);
    }

    // Serialize header with event flags
    serialize_header(sender_lc, 0, tags, flags);

    // BORE/EORE does not contain data - write empty blocks and empty subevent count:
    write_blocks({});
    write_int<std::uint32_t>(0);
}

void FileSerializer::serializeDataRecord(std::string_view sender, const CDTP2Message::DataRecord& data_record) {
    LOG(DEBUG) << "Writing data event " << data_record.getSequenceNumber() << " for " << sender;
    const auto sender_lc = transform(sender, ::tolower);

    serialize_header(sender_lc, data_record.getSequenceNumber(), data_record.getTags());

    const auto write_as_blocks_it = write_as_blocks_.find(sender_lc);
    if(write_as_blocks_it->second) {
        // Interpret multiple blocks as individual blocks of EUDAQ data:

        // Write block data:
        write_blocks(data_record.getBlocks());

        // Zero sub-events:
        write_int<std::uint32_t>(0);
    } else {
        // Interpret each payload block as a EUDAQ sub-event:

        // Write zero blocks:
        write_blocks({});

        // Write subevents:
        const auto& payload = data_record.getBlocks();
        write_int(static_cast<std::uint32_t>(payload.size()));

        for(const auto& block : payload) {
            // Repeat the event header of this event - FIXME adjust event number!
            serialize_header(sender_lc, data_record.getSequenceNumber(), data_record.getTags());

            // Write number of blocks and the block itself
            write_int<std::uint32_t>(1);
            write_block(0, block);

            // Zero sub-sub-events:
            write_int<std::uint32_t>(0);
        }
    }
}
