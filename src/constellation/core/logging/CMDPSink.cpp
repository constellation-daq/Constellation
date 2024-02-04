/**
 * @file
 * @brief Implementation of CMDPSink
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CMDPSink.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

#include <asio.hpp>
#include <magic_enum.hpp>
#include <msgpack.hpp>
#include <zmq.hpp>

#include "constellation/core/logging/Level.hpp"
#include "constellation/core/message/Header.hpp"
#include "constellation/core/utils/casts.hpp"

using namespace constellation;
using namespace constellation::log;
using namespace std::literals::string_literals;
using namespace std::literals::chrono_literals;

// Find path relative to src/, otherwise path without any parent
std::string get_rel_file_path(std::string file_path) {
    const auto src_dir = std::filesystem::path::preferred_separator + "src"s + std::filesystem::path::preferred_separator;
    const auto src_dir_pos = file_path.find(src_dir);
    if(src_dir_pos != std::string::npos) {
        // found /src/, start path after pattern
        file_path = file_path.substr(src_dir_pos + src_dir.length());
    } else {
        // try to find last / for filename
        const auto file_pos = file_path.find_last_of(std::filesystem::path::preferred_separator);
        if(file_pos != std::string::npos) {
            file_path = file_path.substr(file_pos + 1);
        }
    }
    return file_path;
}

// Bind socket to ephemeral port on construction
CMDPSink::CMDPSink() : publisher_(context_, zmq::socket_type::pub), port_(bind_ephemeral_port(publisher_)) {}

void CMDPSink::sink_it_(const spdlog::details::log_msg& msg) {

    // At the very beginning we wait 500ms before starting the async logging.
    // This way the socket can fetch already pending subscriptions
    std::call_once(setup_flag_, []() { std::this_thread::sleep_for(500ms); });

    // Send topic
    auto topic =
        "LOG/" + std::string(magic_enum::enum_name(from_spdlog_level(msg.level))) + "/" + sv_to_string(msg.logger_name);
    std::transform(topic.begin(), topic.end(), topic.begin(), ::toupper);
    publisher_.send(zmq::buffer(topic), zmq::send_flags::sndmore);

    // Fill message header
    auto msghead = message::CMDP1Header(asio::ip::host_name(), msg.time);
    // Add source and thread information only at TRACE level:
    if(msg.level <= spdlog::level::trace) {
        msghead.setTag("thread", static_cast<std::int64_t>(msg.thread_id));
        // Add log source if not empty
        if(!msg.source.empty()) {
            msghead.setTag("filename", get_rel_file_path(msg.source.filename));
            msghead.setTag("lineno", static_cast<std::int64_t>(msg.source.line));
            msghead.setTag("funcname", msg.source.funcname);
        }
    }
    // Send message header
    msgpack::sbuffer sbuf {};
    msgpack::pack(sbuf, msghead);
    zmq::message_t header_frame {sbuf.data(), sbuf.size()};
    publisher_.send(header_frame, zmq::send_flags::sndmore);

    // Pack and send message
    publisher_.send(zmq::const_buffer(msg.payload.data(), msg.payload.size()), zmq::send_flags::none);
}
