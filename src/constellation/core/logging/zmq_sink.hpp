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

#include <asio/ip/host_name.hpp>
#include <zmq.hpp>
#include <magic_enum.hpp>
#include <msgpack.hpp>

#include "MessageHeader.hpp"

namespace Constellation {
    template <typename Mutex> class zmq_sink : public spdlog::sinks::base_sink<Mutex> {
    public:
        zmq_sink() {
            // FIXME get ephemeral port, publish port to CHIRP
            publisher_.bind("tcp://*:5556");

            // FIXME we need to make sure to pass everything to ZMQ for logging
            this->set_level(spdlog::level::trace);
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {

            // Send topic
            // std::string logger = msg.logger_name;
            std::string topic = "LOG/" + std::string(magic_enum::enum_name(msg.level)) + "/";
            publisher_.send(zmq::buffer(topic), zmq::send_flags::sndmore);

            // Pack and send message header
            auto msghead = MessageHeader(asio::ip::host_name(), msg.time);
            const auto sbuf = msghead.assemble();
            zmq::message_t header_frame {sbuf.data(), sbuf.size()};
            publisher_.send(header_frame, zmq::send_flags::sndmore);

            // Pack and send message
            std::map<std::string, msgpack::type::variant> payload;
            // payload["msg"] = std::string(msg.payload);
            payload["thread"] = msg.thread_id;
            payload["filename"] = msg.source.filename;
            payload["lineno"] = msg.source.line;
            payload["funcname"] = msg.source.funcname;
            msgpack::sbuffer mbuf {};
            msgpack::pack(mbuf, payload);
            zmq::message_t payload_frame{mbuf.data(), mbuf.size()};
            publisher_.send(payload_frame, zmq::send_flags::none);
        }

        void flush_() override {}
    private:
        zmq::context_t context_{};
        zmq::socket_t publisher_{context_, zmq::socket_type::pub};
    };

    // TODO: mt even needed? ZeroMQ should be thread safe...
    using zmq_sink_mt = zmq_sink<std::mutex>;
    using zmq_sink_st = zmq_sink<spdlog::details::null_mutex>;
} // namespace Constellation
