/**
 * @file
 * @brief Handler for CSCP
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <zmq.hpp>

#include "constellation/core/message/CSCP1Message.hpp"

namespace constellation::satellite {

    class CSCPHandler {
    public:
        CSCPHandler();

    private:
        message::CSCP1Message getNextCommand();
        void sendReply(message::CSCP1Message& reply);

    private:
        zmq::context_t context_;
        zmq::socket_t rep_;
    };

} // namespace constellation::satellite
