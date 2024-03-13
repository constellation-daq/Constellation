/**
 * @file
 * @brief Implementation of CSCP handler
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CSCPHandler.hpp"

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/message/CSCP1Message.hpp"

using namespace constellation::message;
using namespace constellation::satellite;

CSCPHandler::CSCPHandler() : rep_(context_, zmq::socket_type::rep) {}

CSCP1Message CSCPHandler::getNextCommand() {
    // Receive next message
    zmq::multipart_t recv_msg {};
    recv_msg.recv(rep_);

    // TODO(stephan.lachnit): check for timeout

    auto message = CSCP1Message::disassemble(recv_msg);

    // TODO(stephan.lachnit): check if throws

    return message;
}

void CSCPHandler::sendReply(CSCP1Message& reply) {
    // Assemble to zmq::multipart_t and send
    reply.assemble().send(rep_);
}
