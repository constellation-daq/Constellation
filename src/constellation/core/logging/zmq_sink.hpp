/**
 * @file
 * @brief Log sink for ZMQ communication
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <variant>

#include "spdlog/details/null_mutex.h"
#include "spdlog/sinks/base_sink.h"

#include <asio.hpp>
#include <magic_enum.hpp>
#include <msgpack.hpp>
#include <zmq.hpp>

#include "constellation/core/message/Header.hpp"
#include "LogLevel.hpp"

namespace constellation {
    template <typename Mutex> class zmq_sink : public spdlog::sinks::base_sink<Mutex> {
    public:
        zmq_sink() {
            // FIXME get ephemeral port, publish port to CHIRP
            publisher_.bind("tcp://*:5556");
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            // Send topic
            auto topic = "LOG/" + std::string(magic_enum::enum_name(static_cast<LogLevel>(msg.level))) + "/" +
                         std::string(msg.logger_name.data(), msg.logger_name.size());
            std::transform(topic.begin(), topic.end(), topic.begin(), ::toupper);
            publisher_.send(zmq::buffer(topic), zmq::send_flags::sndmore);

            // Pack and send message header
            auto msghead = message::CMDP1Header(asio::ip::host_name(), msg.time);
            const auto sbuf = msghead.assemble();
            zmq::message_t header_frame {sbuf.data(), sbuf.size()};
            publisher_.send(header_frame, zmq::send_flags::sndmore);

            // Pack and send message
            dictionary_t payload;
            payload["msg"] = std::string(msg.payload.data(), msg.payload.size());

            // Add source and thread information only at TRACE level:
            if(msg.level <= spdlog::level::trace) {
                payload["thread"] = msg.thread_id;
                // Add log source if not empty
                if(!msg.source.empty()) {
                    payload["filename"] = msg.source.filename;
                    payload["lineno"] = msg.source.line;
                    payload["funcname"] = msg.source.funcname;
                }
            }

            msgpack::sbuffer mbuf {};
            msgpack::pack(mbuf, payload);
            zmq::message_t payload_frame {mbuf.data(), mbuf.size()};
            publisher_.send(payload_frame, zmq::send_flags::none);
        }

        void flush_() override {}

    private:
        zmq::context_t context_ {};
        zmq::socket_t publisher_ {context_, zmq::socket_type::pub};
    };

    // TODO: mt even needed? ZeroMQ should be thread safe...
    using zmq_sink_mt = zmq_sink<std::mutex>;
    using zmq_sink_st = zmq_sink<spdlog::details::null_mutex>;
} // namespace constellation
