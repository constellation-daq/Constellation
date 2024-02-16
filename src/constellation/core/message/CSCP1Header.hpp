/**
 * @file
 * @brief Message header for CSCP1
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "constellation/core/message/Header.hpp"
#include "constellation/core/message/Protocol.hpp"

namespace constellation::message {

    /** CSCP1 Header */
    class CSCP1Header final : public Header {
    public:
        CSCP1Header(std::string sender, std::chrono::system_clock::time_point time = std::chrono::system_clock::now())
            : Header(CSCP1, std::move(sender), time) {}

        static CSCP1Header disassemble(std::span<const std::byte> data) { return {Header::disassemble(CSCP1, data)}; }

    private:
        CSCP1Header(Header&& base_header) : Header(std::move(base_header)) {}
    };

} // namespace constellation::message
