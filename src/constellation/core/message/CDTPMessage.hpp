/**
 * @file
 * @brief Message class for CDTP
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
#include <vector>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config.hpp"
#include "constellation/core/message/Header.hpp"

namespace constellation::message {

    class CDTPMessage {
    public:
        CNSTLN_API CDTPMessage(CDTP1Header header, size_t frames = 1);

        constexpr const CDTP1Header& getHeader() const { return header_; }

        std::vector<std::shared_ptr<zmq::message_t>> getPayload() const { return payload_frames_; }

        void addPayload(std::shared_ptr<zmq::message_t> payload) { payload_frames_.emplace_back(std::move(payload)); }

        /**
         * Assemble full message to frames for ZeroMQ
         *
         * This function moves the payload.
         */
        CNSTLN_API zmq::multipart_t assemble();

        /**
         * Disassemble message from ZeroMQ frames
         *
         * This function moves the payload frames
         */
        CNSTLN_API static CDTPMessage disassemble(zmq::multipart_t& frames);

    private:
        CDTP1Header header_;
        std::vector<std::shared_ptr<zmq::message_t>> payload_frames_;
    };

} // namespace constellation::message
