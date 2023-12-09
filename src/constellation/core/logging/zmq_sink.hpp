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

#include "spdlog/details/null_mutex.h"
#include "spdlog/sinks/base_sink.h"

// TODO implement

namespace Constellation {
    template <typename Mutex> class zmq_sink : public spdlog::sinks::base_sink<Mutex> {
    public:
        zmq_sink() {}

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            // Format log message
            spdlog::memory_buf_t formatted {};
            spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
            auto msg_fmt = fmt::to_string(formatted);

            // Send msg_fmt over ZeroMQ
            // TODO
        }

        void flush_() override {}
    };

    // TODO: mt even needed? ZeroMQ should be thread safe...
    using zmq_sink_mt = zmq_sink<std::mutex>;
    using zmq_sink_st = zmq_sink<spdlog::details::null_mutex>;
} // namespace Constellation
