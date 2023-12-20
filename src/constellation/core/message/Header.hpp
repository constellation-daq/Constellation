/**
 * @file
 * @brief CMDP Message Header
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <map>
#include <span>
#include <string>
#include <string_view>

#include <msgpack.hpp> // https://github.com/msgpack/msgpack-c/blob/cpp_master/

#include "constellation/core/utils/dictionary.h"

using namespace std::literals::string_view_literals;

constexpr std::string_view CMDP1_PROTOCOL = "CMDP\01"sv;

namespace Constellation::Message {

    // Note: we might want to have the protocol as a template argument for the class if we reuse it for CDTP as well
    class Header {

    public:
        Header(std::string_view sender, std::chrono::system_clock::time_point time) : sender_(sender), time_(time) {}
        Header(std::string_view sender) : sender_(sender), time_(std::chrono::system_clock::now()) {}

        // Reconstruct from bytes
        Header(std::span<char> data);

        std::chrono::system_clock::time_point getTime() const { return time_; }
        std::string_view getSender() const { return sender_; }
        dictionary_t getTags() const { return tags_; }

        template <typename T> T getTag(const std::string& key) const { return tags_.at(key); }

        template <typename T> void setTag(const std::string& key, T value) { tags_[key] = value; }

        msgpack::sbuffer assemble() const;

        void print() const;

    private:
        std::string sender_;
        std::chrono::system_clock::time_point time_;
        dictionary_t tags_;
    };

} // namespace Constellation::Message
