/**
 * @file
 * @brief Message header for CDTP1
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <string>

#include "constellation/core/config.hpp"
#include "constellation/core/message/Dictionary.hpp"
#include "constellation/core/message/Header.hpp"
#include "constellation/core/message/Protocol.hpp"

namespace constellation::message {

    enum class CDTP1Type : std::uint8_t {
        DATA = '\x00',
        BOR = '\x01',
        EOR = '\x02',
    };
    using enum CDTP1Type;

    class CNSTLN_API CDTP1Header final : public Header {
    public:
        CDTP1Header(std::string sender,
                    std::uint64_t seq,
                    CDTP1Type type,
                    std::chrono::system_clock::time_point time = std::chrono::system_clock::now())
            : Header(CDTP1, std::move(sender), time), seq_(seq), type_(type) {}

        constexpr std::uint64_t getSequenceNumber() const { return seq_; }

        constexpr CDTP1Type getType() const { return type_; }

        CNSTLN_API std::string to_string() const final;

        CNSTLN_API static CDTP1Header disassemble(std::span<const std::byte> data);

        CNSTLN_API void msgpack_pack(msgpack::packer<msgpack::sbuffer>& msgpack_packer) const final;

    private:
        CDTP1Header(std::string sender,
                    std::chrono::system_clock::time_point time,
                    Dictionary tags,
                    std::uint64_t seq,
                    CDTP1Type type)
            : Header(CDTP1, std::move(sender), time, std::move(tags)), seq_(seq), type_(type) {}

    private:
        std::uint64_t seq_;
        CDTP1Type type_;
    };

} // namespace constellation::message
