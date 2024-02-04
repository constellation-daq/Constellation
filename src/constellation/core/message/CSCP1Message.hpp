/**
 * @file
 * @brief Message class for CSCP1
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config.hpp"
#include "constellation/core/message/Header.hpp"

namespace constellation::message {

    /** Enum describing the type of CSCP1 message */
    enum class CSCP1Type : std::uint8_t {
        /** Request with a command */
        REQUEST = '\x00',

        /** Command is being executed */
        SUCCESS = '\x01',

        /** Command is valid but not implemented */
        NOTIMPLEMENTED = '\x02',

        /** Command is valid but mandatory payload information is missing or incorrectly formatted */
        INCOMPLETE = '\x03',

        /** Command is invalid for the current state */
        INVALID = '\x04',

        /** Command is entirely unknown */
        UNKNOWN = '\x05',
    };

    /** Class representing a CSCP1 message */
    class CSCP1Message {
    public:
        /**
         * @param header CSCP1 header of the message
         * @param verb Message verb containing the type and the command/reply string
         */
        CNSTLN_API CSCP1Message(CSCP1Header header, std::pair<CSCP1Type, std::string> verb);

        /**
         * @return CSCP1 header of the message
         */
        constexpr const CSCP1Header& getHeader() const { return header_; }

        /**
         * @return Message verb containing the type and the command/reply string
         */
        std::pair<CSCP1Type, std::string_view> getVerb() const { return verb_; }

        /**
         * @return Message payload
         */
        std::shared_ptr<const zmq::message_t> getPayload() const { return payload_; }

        /**
         * @param payload Shared pointer to the ZeroMQ message to be used as payload
         */
        void addPayload(std::shared_ptr<zmq::message_t> payload) { payload_ = std::move(payload); }

        /**
         * Assemble full message to frames for ZeroMQ
         *
         * This function moves the payload.
         *
         * @return Message assembled to ZeroMQ frames
         */
        CNSTLN_API zmq::multipart_t assemble();

        /**
         * Disassemble message from ZeroMQ frames
         *
         * This function moves the payload frame if there is one.
         *
         * @return New CSCP1Message assembled from ZeroMQ frames
         * @throw TODO
         */
        CNSTLN_API static CSCP1Message disassemble(zmq::multipart_t& frames);

    private:
        CSCP1Header header_;
        std::pair<CSCP1Type, std::string> verb_;
        std::shared_ptr<zmq::message_t> payload_;
    };

} // namespace constellation::message
