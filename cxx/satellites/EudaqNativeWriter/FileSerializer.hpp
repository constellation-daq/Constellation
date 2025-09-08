/**
 * @file
 * @brief EUDAQ data serializer
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CDTP2Message.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"
#include "constellation/core/utils/string_hash_map.hpp"

/** Serializer class for EUDAQ native binary files */
class FileSerializer {
private:
    /** Event flags */
    enum class EUDAQFlags : std::uint32_t { // NOLINT(performance-enum-size)
        BORE = 0x1,
        EORE = 0x2,
        TRIGGER = 0x10,
    };

public:
    /**
     * @brief Constructor for file serializer
     * This attempts to open the file and checks if the path already exists
     *
     * @param file Output file stream to write to
     * @param run_sequence Sequence portion of the run identifier, used to store in EUDAQ event header
     */
    FileSerializer(std::ofstream file, std::uint32_t run_sequence);

    /**
     * @brief Destructor which flushes data to file and closes the file
     */
    virtual ~FileSerializer();

    /// @cond doxygen_suppress
    // No copy/move constructor/assignment
    FileSerializer(const FileSerializer& other) = delete;
    FileSerializer& operator=(const FileSerializer& other) = delete;
    FileSerializer(FileSerializer&& other) = delete;
    FileSerializer& operator=(FileSerializer&& other) = delete;
    /// @endcond

    /**
     * @brief Flush newly written content to disk
     */
    void flush();

    /**
     * @brief Function to serialize BOR and EOR messages which delimit a run
     * @details This function serializes the BOR and EOR delimiter CDTP messages into EUDAQ native binary format. The
     * corresponding EUDAQ events are marked as BORE and EORE, respectively. The header of the EUDAQ event will contain
     * the payload of the corresponding CDTP message, so the satellite configuration for the BOR and the run metadata for
     * the EOR, the header tags of the CDTP messages are not stored.
     *
     * Some file serializer settings are taken from the header tags for the BOR event to configure the further treatment
     * of the data.
     *
     * @param sender Sender name
     * @param type Message type
     * @param tags Framework and user tags
     */
    void serializeDelimiterMsg(std::string_view sender,
                               constellation::message::CDTP2Message::Type type,
                               const constellation::config::Dictionary& tags);

    /**
     * @brief Function to serialize data record
     * @details This function takes Constellation CDTP data records and serializes them to the EUDAQ native binary
     * format. It first serializes header information of the event and then writes the data blocks either as blocks
     * of the event or as sub-events, depending on the settings. For sub-events, the dictionary of the data record
     * is repeated.
     *
     * @param sender Canonical name of the sending satellite
     * @param data_record CDTP data record
     */
    void serializeDataRecord(std::string_view sender, const constellation::message::CDTP2Message::DataRecord& data_record);

private:
    /**
     * @brief Function to serialize a EUDAQ event header
     *
     * @param sender_lc Low-case sender name
     * @param sequence_number Message sequence number
     * @param tags Tags to be serialized, either header tags of payload dictionaries
     * @param flags EUDAQ flags to mark the event with
     */
    void serialize_header(std::string_view sender_lc,
                          std::uint64_t sequence_number,
                          const constellation::config::Dictionary& tags,
                          std::uint32_t flags = 0x0);

    /** Set eudaq event descriptors and block handling from BOR tags */
    void parse_bor_tags(std::string_view sender, const constellation::config::Dictionary& user_tags);

    /** Write data to file */
    void write(std::span<const std::byte> data);

    /** Write EUDAQ event data records to file */
    void write_blocks(const std::vector<constellation::message::PayloadBuffer>& payload);

    /** Write a single EUDAQ event data record to file */
    void write_block(std::uint32_t key, const constellation::message::PayloadBuffer& payload);

    /** Write integers of different sizes to file */
    template <typename T> void write_int(T t) {
        static_assert(sizeof(t) > 1, "Only supports integers of size > 1 byte");
        std::array<std::byte, sizeof t> buf;
        for(std::size_t i = 0; i < sizeof t; ++i) {
            buf[i] = static_cast<std::byte>(t & 0xff);
            t >>= 8;
        }
        write({buf.data(), buf.size()});
    }

    /** Write a string to file */
    void write_str(std::string_view t);

    /** Write a dictionary to file */
    void write_tags(const constellation::config::Dictionary& dict);

    /** Helper function to hash a string into EUDAQ event identifiers */
    constexpr std::uint32_t cstr2hash(const char* str, std::uint32_t h = 0) { // NOLINT(misc-no-recursion)
        return !str[h] ? 5381 : (cstr2hash(str, h + 1) * 33ULL) ^ str[h];     // NOLINT
    }

private:
    std::ofstream file_;
    std::uint32_t run_sequence_;

    constellation::utils::string_hash_map<std::string> eudaq_event_descriptors_;
    constellation::utils::string_hash_map<bool> write_as_blocks_;
};
