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

#include <msgpack/pack_decl.hpp>
#include <msgpack/sbuffer_decl.hpp>

#include "constellation/core/config.hpp"
#include "constellation/core/message/Dictionary.hpp"
#include "constellation/core/message/Protocol.hpp"

namespace constellation::message {

    /** Message Header */
    template <Protocol P> class Header {
    public:
        /**
         * Construct new message header
         *
         * @param sender Sender name
         * @param time Message time (defaults to current time)
         */
        Header(std::string sender, std::chrono::system_clock::time_point time = std::chrono::system_clock::now())
            : sender_(std::move(sender)), time_(time) {}

        /** Return message time */
        constexpr std::chrono::system_clock::time_point getTime() const { return time_; }

        /** Return message sender */
        constexpr std::string_view getSender() const { return sender_; }

        /** Return message tags */
        constexpr const Dictionary& getTags() const { return tags_; }

        /** Return message tag */
        constexpr DictionaryValue getTag(const std::string& key) const { return tags_.at(key); }

        /** Set message tag */
        constexpr void setTag(const std::string& key, DictionaryValue value) { tags_[key] = std::move(value); }

        /** Convert message header to human readable string */
        CNSTLN_API std::string to_string() const;

        /** Pack message header with msgpack */
        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const;

        /**
         * Disassemble message from from bytes
         *
         * @param data View to byte data
         */
        CNSTLN_API static Header disassemble(std::span<const std::byte> data);

    private:
        std::string sender_;
        std::chrono::system_clock::time_point time_;
        Dictionary tags_;
    };

    /** CSCP1 Header */
    using CSCP1Header = Header<CSCP1>;

    /** CMDP1 Header */
    using CMDP1Header = Header<CMDP1>;

    /** CDTP1 Header */
    using CDTP1Header = Header<CDTP1>;

} // namespace constellation::message
