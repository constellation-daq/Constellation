/**
 * @file
 * @brief Message Header for CMDP, CDTP and CSCP
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <span>
#include <string_view>

#include <msgpack.hpp>

#include "constellation/core/config.hpp"
#include "constellation/core/message/Protocol.hpp"
#include "constellation/core/utils/dictionary.hpp"

namespace constellation::message {

    /** Message Header */
    template <Protocol P> class Header {
    public:
        /**
         * Construct new message header
         *
         * @param sender Sender name
         * @param time Message time
         */
        CNSTLN_API Header(std::string_view sender, std::chrono::system_clock::time_point time);

        /**
         * Construct new message header using current time
         *
         * @param sender Sender name
         */
        CNSTLN_API Header(std::string_view sender);

        /**
         * Construct new message header from bytes
         *
         * @param data View to byte data
         */
        CNSTLN_API Header(std::span<std::byte> data);

        /** Return message time */
        constexpr std::chrono::system_clock::time_point getTime() const { return time_; }

        /** Return message sender */
        constexpr std::string_view getSender() const { return sender_; }

        /** Return message tags */
        CNSTLN_API dictionary_t getTags() const { return tags_; }

        /** Return message tag */
        template <typename T> T constexpr getTag(const std::string& key) const { return tags_.at(key); }

        /** Set message tag */
        template <typename T> constexpr void setTag(const std::string& key, T value) { tags_[key] = value; }

        /** Assemble message header to bytes */
        CNSTLN_API msgpack::sbuffer assemble() const;

        /** Print message header to std::cout */
        CNSTLN_API std::string to_string() const;

    private:
        std::string sender_;
        std::chrono::system_clock::time_point time_;
        dictionary_t tags_;
    };

    /** CSCP1 Header */
    using CSCP1Header = Header<CSCP1>;

    /** CMDP1 Header */
    using CMDP1Header = Header<CMDP1>;

    /** CDTP1 Header */
    using CDTP1Header = Header<CDTP1>;

} // namespace constellation::message
