/**
 * @file
 * @brief Message class for CDTP2
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <msgpack.hpp>
#include <zmq_addon.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/PayloadBuffer.hpp"

namespace constellation::message {
    /**
     * @brief Class representing a CDTP2 message
     */
    class CDTP2Message {
    public:
        /**
         * @brief Enum describing the type of the CDTP message
         */
        enum class Type : std::uint8_t {
            /** Message containing data */
            DATA = '\x00',

            /** Message containing begin-of-run information */
            BOR = '\x01',

            /** Message containing end-of-run information */
            EOR = '\x02',
        };

        /**
         * @brief Data record representing a data point
         */
        class DataRecord {
        public:
            DataRecord() = default;

            /**
             * @brief Constructs a data record
             *
             * @param sequence_number Sequence number of the data record
             * @param tags Dictionary containing metainformation of the data record
             * @param blocks Optional number of blocks to reserve
             */
            DataRecord(std::uint64_t sequence_number, config::Dictionary tags, std::size_t blocks = 1)
                : sequence_number_(sequence_number), tags_(std::move(tags)) {
                blocks_.reserve(blocks);
            }

            /**
             * @brief Get the sequence number of the data record
             *
             * @return Sequence number
             */
            std::uint64_t getSequenceNumber() const { return sequence_number_; }

            /**
             * @brief Get the dictionary containing the metainformation of the data record
             *
             * @return Tags of the data record
             */
            const config::Dictionary& getTags() const { return tags_; }

            /**
             * @brief Add a tag to the metainformation of the data record
             *
             * @param key Name of the tag
             * @param value Value of the tag
             */
            template <typename T> void addTag(const std::string& key, const T& value) { tags_[key] = value; }

            /**
             * @brief Get the attached blocks of the data record
             *
             * @return Vector containing payload blocks
             */
            const std::vector<PayloadBuffer>& getBlocks() const { return blocks_; }

            /**
             * @brief Add a block to the data record
             *
             * @param payload Payload buffer to be added as block
             */
            void addBlock(PayloadBuffer&& payload) { blocks_.emplace_back(std::move(payload)); }

            /**
             * @brief Count the number of attached blocks of the data record
             *
             * @return Number of attached blocks
             */
            std::size_t countBlocks() const { return blocks_.size(); }

            /**
             * @brief Count the number of bytes contained in the blocks
             *
             * @return Size of the payload in bytes
             */
            CNSTLN_API std::size_t countPayloadBytes() const;

            void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

            void msgpack_unpack(const msgpack::object& msgpack_object);

        private:
            std::uint64_t sequence_number_ {};
            config::Dictionary tags_;
            std::vector<PayloadBuffer> blocks_;
        };

    public:
        /**
         * @brief Construct a CDTP2 message
         *
         * @param sender Name of the sender
         * @param type CDTP message type
         * @param blocks Optional number of blocks to reserve
         */
        CDTP2Message(std::string sender, Type type, std::size_t blocks = 1) : sender_(std::move(sender)), type_(type) {
            data_records_.reserve(blocks);
        }

        /**
         * @brief Get the name of the sender
         *
         * @return Name of the sender
         */
        std::string_view getSender() const { return sender_; }

        /**
         * @brief Get the message type
         *
         * @return Message type
         */
        Type getType() const { return type_; }

        /**
         * @brief Get the data records attached to the message
         *
         * @return Vector containing the data records
         */
        const std::vector<DataRecord>& getDataRecords() const { return data_records_; }

        /**
         * @brief Add a data record to the message
         *
         * @param data_record Data record
         */
        void addDataRecord(DataRecord&& data_record) { data_records_.emplace_back(std::move(data_record)); }

        /**
         * @brief Count the number of payload bytes contained in each data record
         *
         * @return Size of the payload in bytes
         */
        CNSTLN_API std::size_t countPayloadBytes() const;

        /**
         * @brief Clear data records attached to message
         */
        void clearBlocks() { data_records_.clear(); }

        /**
         * @brief Assemble full message for ZeroMQ
         *
         * @return ZeroMQ multipart message
         */
        CNSTLN_API zmq::multipart_t assemble() const;

        /**
         * @brief Disassemble message from ZeroMQ frames
         *
         * @param frames ZeroMQ frames
         * @return CDTP2 message
         */
        CNSTLN_API static CDTP2Message disassemble(zmq::multipart_t& frames);

    private:
        std::vector<DataRecord> data_records_;
        std::string sender_;
        Type type_;
    };

    /**
     * @brief Class representing a CDTP2 begin-of-run message
     */
    class CDTP2BORMessage : public CDTP2Message {
    public:
        /**
         * @brief Construct a CDTP2 begin-of-run message
         *
         * @param sender Name of the sender
         * @param user_tags User tags to attach to the message
         * @param configuration Configuration of the sender
         */
        CNSTLN_API CDTP2BORMessage(std::string sender,
                                   config::Dictionary user_tags,
                                   const config::Configuration& configuration);

        /**
         * @brief Construct a CDTP2 begin-of-run message from a CDTP2 message
         *
         * @param message CDTP2 message
         */
        CNSTLN_API CDTP2BORMessage(CDTP2Message&& message);

        /**
         * @brief Get the user tags of the begin-of-run message
         *
         * @return Dictionary containing the user tags
         */
        const config::Dictionary& getUserTags() const { return getDataRecords().at(0).getTags(); }

        /**
         * @brief Get the configuration of the sender
         *
         * @return Configuration
         */
        CNSTLN_API config::Configuration getConfiguration() const;

    private:
        using CDTP2Message::addDataRecord;
        using CDTP2Message::getDataRecords;
    };

    /**
     * @brief Class representing a CDTP2 end-of-run message
     */
    class CDTP2EORMessage : public CDTP2Message {
    public:
        /**
         * @brief Construct a CDTP2 end-of-run message
         *
         * @param sender Name of the sender
         * @param user_tags User tags to attach to the message
         * @param run_metadata Run metadata
         */
        CNSTLN_API CDTP2EORMessage(std::string sender, config::Dictionary user_tags, config::Dictionary run_metadata);

        /**
         * @brief Construct a CDTP2 end-of-run message from a CDTP2 message
         *
         * @param message CDTP2 message
         */
        CNSTLN_API CDTP2EORMessage(CDTP2Message&& message);

        /**
         * @brief Get the user tags of the end-of-run message
         *
         * @return Dictionary containing the user tags
         */
        const config::Dictionary& getUserTags() const { return getDataRecords().at(0).getTags(); }

        /**
         * @brief Get the run metadata
         *
         * @return Dictionary containing the run metadata
         */
        const config::Dictionary& getRunMetadata() const { return getDataRecords().at(1).getTags(); }

    private:
        using CDTP2Message::addDataRecord;
        using CDTP2Message::getDataRecords;
    };

} // namespace constellation::message
