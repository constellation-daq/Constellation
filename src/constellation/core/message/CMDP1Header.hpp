/**
 * @file
 * @brief Message header for CMDP1
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "constellation/core/message/Header.hpp"
#include "constellation/core/message/Protocol.hpp"

namespace constellation::message {

    /** CMDP1 Header */
    class CMDP1Header final : public Header {
    public:
        CMDP1Header(std::string sender, std::chrono::system_clock::time_point time = std::chrono::system_clock::now())
            : Header(CMDP1, std::move(sender), time) {}

        static CMDP1Header disassemble(std::span<const std::byte> data) { return {Header::disassemble(CMDP1, data)}; }

    private:
        CMDP1Header(Header&& base_header) : Header(std::move(base_header)) {}
    };

} // namespace constellation::message