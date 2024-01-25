/**
 * @file
 * @brief Log sink for ZMQ communication
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <mutex>

#include <spdlog/sinks/base_sink.h>
#include <zmq.hpp>

#include "constellation/core/utils/ports.hpp"

namespace constellation::log {
    /**
     * Sink log messages via CMDP
     *
     * Note that ZeroMQ sockets are not thread-safe, meaning that the sink requires a mutex.
     */
    class CMDPSink : public spdlog::sinks::base_sink<std::mutex> {
    public:
        /**
         * Construct a new CMDPSink
         */
        CMDPSink();

        /**
         * Get ephemeral port this logger sink is bound to
         *
         * @return Port number
         */
        constexpr Port getPort() const { return port_; }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) final;
        void flush_() final {}

    private:
        zmq::context_t context_;
        zmq::socket_t publisher_;
        Port port_;
    };

} // namespace constellation::log
