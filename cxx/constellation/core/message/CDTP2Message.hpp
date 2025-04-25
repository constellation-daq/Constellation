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
         * @brief Data block representing a data point
         */
        class DataBlock {
        public:
            DataBlock() = default;

            /**
             * @brief Constructs a data block
             *
             * @param sequence_number Sequence number of the data block
             * @param tags Dictionary containing metainformation of the data block
             * @param frames Optional number of frames to reserve
             */
            DataBlock(std::uint64_t sequence_number, config::Dictionary tags, std::size_t frames = 1)
                : sequence_number_(sequence_number), tags_(std::move(tags)) {
                frames_.reserve(frames);
            }

            /**
             * @brief Get the sequence number of the data block
             *
             * @return Sequence number
             */
            std::uint64_t getSequenceNumber() const { return sequence_number_; }

            /**
             * @brief Get the dictionary containing the metainformation of the data block
             *
             * @return Tags of the data block
             */
            const config::Dictionary& getTags() const { return tags_; }

            /**
             * @brief Add a tag to the metainformation of the data block
             *
             * @param key Name of the tag
             * @param value Value of the tag
             */
            template <typename T> void addTag(const std::string& key, const T& value) { tags_[key] = value; }

            /**
             * @brief Get the attached frames of the data block
             *
             * @return Vector containing payload frames
             */
            const std::vector<PayloadBuffer>& getFrames() const { return frames_; }

            /**
             * @brief Add a frame to the data block
             *
             * @param payload Payload buffer to be added as frame
             */
            void addFrame(PayloadBuffer&& payload) { frames_.emplace_back(std::move(payload)); }

            /**
             * @brief Count the number of bytes contained in the frames
             *
             * @return Size of the payload in bytes
             */
            CNSTLN_API std::size_t countPayloadBytes() const;

            void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

            void msgpack_unpack(const msgpack::object& msgpack_object);

        private:
            std::uint64_t sequence_number_ {};
            config::Dictionary tags_;
            std::vector<PayloadBuffer> frames_;
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
            data_blocks_.reserve(blocks);
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
         * @brief Get the data blocks attached to the message
         *
         * @return Vector containing the data blocks
         */
        const std::vector<DataBlock>& getDataBlocks() const { return data_blocks_; }

        /**
         * @brief Add a data block to the message
         *
         * @param data_block Data block
         */
        void addDataBlock(DataBlock&& data_block) { data_blocks_.emplace_back(std::move(data_block)); }

        /**
         * @brief Count the number of payload bytes contained in each data block
         *
         * @return Size of the payload in bytes
         */
        CNSTLN_API std::size_t countPayloadBytes() const;

        /**
         * @brief Clear data blocks attached to message
         */
        void clearBlocks() { data_blocks_.clear(); }

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
        std::vector<DataBlock> data_blocks_;
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
        const config::Dictionary& getUserTags() const { return getDataBlocks().at(0).getTags(); }

        /**
         * @brief Get the configuration of the sender
         *
         * @return Configuration
         */
        CNSTLN_API config::Configuration getConfiguration() const;

    private:
        using CDTP2Message::addDataBlock;
        using CDTP2Message::getDataBlocks;
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
        const config::Dictionary& getUserTags() const { return getDataBlocks().at(0).getTags(); }

        /**
         * @brief Get the run metadata
         *
         * @return Dictionary containing the run metadata
         */
        const config::Dictionary& getRunMetadata() const { return getDataBlocks().at(1).getTags(); }

    private:
        using CDTP2Message::addDataBlock;
        using CDTP2Message::getDataBlocks;
    };

} // namespace constellation::message
