/**
 * @file
 * @brief ZeroMQ helper functions
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/networking/Port.hpp"

// TODO(stephan.lachnit): add version check once new release with https://github.com/zeromq/cppzmq/pull/648 is available
namespace zmq::sockopt {
#ifdef ZMQ_BUSY_POLL
    ZMQ_DEFINE_INTEGRAL_BOOL_UNIT_OPT(ZMQ_BUSY_POLL, busy_poll, int);
#endif
} // namespace zmq::sockopt

namespace constellation::networking {

    /**
     * @brief Bind ZeroMQ socket to wildcard address with ephemeral port
     *
     * See also https://libzmq.readthedocs.io/en/latest/zmq_tcp.html.
     *
     * @param socket Reference to socket which should be bound
     * @return Ephemeral port assigned by operating system
     */
    CNSTLN_API Port bind_ephemeral_port(zmq::socket_t& socket);

    /**
     * @brief Return the global ZeroMQ context
     *
     * @note Since the global ZeroMQ context is static, static classes need to store an instance of the shared pointer.
     *
     * @return Shared pointer to the global ZeroMQ context
     */
    CNSTLN_API std::shared_ptr<zmq::context_t>& global_zmq_context();

} // namespace constellation::networking
