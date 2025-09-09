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
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <spdlog/async_logger.h>
#include <spdlog/details/log_msg.h>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/metrics/MetricsManager.hpp"
#include "constellation/core/networking/exceptions.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/core/utils/string_hash_map.hpp"
#include "constellation/core/utils/thread.hpp"
#include "constellation/core/utils/windows.hpp"

using namespace constellation::config;
using namespace constellation::log;
using namespace constellation::message;
using namespace constellation::metrics;
using namespace constellation::networking;
using namespace constellation::protocol;
using namespace constellation::utils;
using namespace std::chrono_literals;

namespace {
    // Find path relative to cxx/, otherwise path without any parent
    std::string get_rel_file_path(std::string file_path_char) {
        auto file_path = to_platform_string(std::move(file_path_char));
        const auto src_dir = std::filesystem::path::preferred_separator + to_platform_string("cxx") +
                             std::filesystem::path::preferred_separator;
        const auto src_dir_pos = file_path.find(src_dir);
        if(src_dir_pos != std::filesystem::path::string_type::npos) {
            // found /cxx/, start path after pattern
            file_path = file_path.substr(src_dir_pos + src_dir.length());
        } else {
            // try to find last / for filename
            const auto file_pos = file_path.find_last_of(std::filesystem::path::preferred_separator);
            if(file_pos != std::filesystem::path::string_type::npos) {
                file_path = file_path.substr(file_pos + 1);
            }
        }
        return to_std_string(std::move(file_path));
    }
} // namespace

CMDPSink::CMDPSink()
    : global_context_(global_zmq_context()), pub_socket_(*global_context_, zmq::socket_type::xpub),
      port_(bind_ephemeral_port(pub_socket_)) {
    // Set reception timeout for subscription messages on XPUB socket to zero because we need to mutex-lock the socket
    // while reading and cannot log at the same time.
    try {
        pub_socket_.set(zmq::sockopt::rcvtimeo, 0);
    } catch(const zmq::error_t& e) {
        throw NetworkError(e.what());
    }
}

void CMDPSink::subscription_loop(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {

        // Receive subscription message
        zmq::multipart_t recv_msg {};
        bool received = false;

        try {
            // Lock for the mutex provided by the sink base class
            const std::lock_guard socket_lock {mutex_};
            received = recv_msg.recv(pub_socket_);
        } catch(const zmq::error_t& e) {
            throw NetworkError(e.what());
        }

        // Return if timed out or wrong number of frames received
        if(!received || recv_msg.size() != 1) {
            // Only check every 100ms for new subscription messages
            std::this_thread::sleep_for(100ms);
            continue;
        }

        const auto& frame = recv_msg.front();

        // First byte \x01 is subscription, \0x00 is unsubscription
        const auto subscribe = static_cast<bool>(*frame.data<uint8_t>());

        // Log topic is message body stripped by first byte
        auto body = frame.to_string_view();
        body.remove_prefix(1);
        LOG(*logger_, TRACE) << "Received " << (subscribe ? "" : "un") << "subscribe message for " << body;

        // Handle subscriptions as well as notification subscriptions:
        if(body.starts_with("LOG/")) {
            handle_log_subscriptions(subscribe, body);
        } else if(body.starts_with("LOG?")) {
            if(subscribe) {
                ManagerLocator::getSinkManager().sendLogNotification();
            }
        } else if(body.starts_with("STAT/")) {
            handle_stat_subscriptions(subscribe, body);
        } else if(body.starts_with("STAT?")) {
            if(subscribe) {
                ManagerLocator::getSinkManager().sendMetricNotification();
            }
        } else {
            LOG(*logger_, WARNING) << "Received " << (subscribe ? "" : "un") << "subscribe message with invalid topic "
                                   << body << ", ignoring";
        }
    }
}

void CMDPSink::handle_log_subscriptions(bool subscribe, std::string_view body) {
    // Find log level
    const auto level_endpos = body.find_first_of('/', 4);
    const auto level_str = body.substr(4, level_endpos - 4);

    // Empty level means subscription to everything
    const auto level = (level_str.empty() ? std::optional<Level>(TRACE) : enum_cast<Level>(level_str));

    // Only accept valid levels
    if(!level.has_value()) {
        LOG(*logger_, TRACE) << "Invalid log level " << quote(level_str) << ", ignoring";
        return;
    }

    // Extract topic
    const auto topic = (level_endpos != std::string_view::npos ? body.substr(level_endpos + 1) : std::string_view());
    const auto topic_uc = transform(topic, ::toupper);

    // Adjust subscription counter
    LOG(*logger_, TRACE) << (subscribe ? "In" : "De") << "crementing subscription counter for topic " << quote(topic_uc);
    // Note: new counter automatically initialized to zero
    auto& counter = log_subscriptions_[topic_uc][level.value()];
    if(subscribe) {
        counter += 1;
    } else if(counter > 0) {
        counter -= 1;
    }

    // Figure out lowest level for each topic
    auto cmdp_global_level = Level::OFF;
    string_hash_map<Level> cmdp_sub_topic_levels {};
    cmdp_sub_topic_levels.reserve(log_subscriptions_.size());
    for(const auto& [logger, levels] : log_subscriptions_) {
        auto it = std::ranges::find_if(levels, [](const auto& i) { return i.second > 0; });
        if(it != levels.end()) {
            if(!logger.empty()) {
                cmdp_sub_topic_levels[logger] = it->first;
            } else {
                cmdp_global_level = it->first;
            }
        }
    }

    LOG(*logger_, TRACE) << "Lowest global log level: " << quote(enum_name(cmdp_global_level));

    // Update subscriptions
    ManagerLocator::getSinkManager().updateCMDPLevels(cmdp_global_level, std::move(cmdp_sub_topic_levels));
}

void CMDPSink::handle_stat_subscriptions(bool subscribe, std::string_view body) {
    // Find stat topic
    const auto topic = body.substr(5);
    const auto topic_uc = transform(topic, ::toupper);

    // Adjust subcrption counter
    LOG(*logger_, TRACE) << (subscribe ? "In" : "De") << "crementing subscription counter for topic " << quote(topic_uc);
    // Note: new counter automatically initialized to zero
    auto& counter = stat_subscriptions_[topic_uc];
    if(subscribe) {
        counter += 1;
    } else if(counter > 0) {
        counter -= 1;
    }

    // Global subscription to all topics
    const auto global_it = stat_subscriptions_.find("");
    const auto global_subscription = (global_it != stat_subscriptions_.cend() && global_it->second > 0);

    // List of subscribed topics:
    string_hash_set subscription_topics {};
    subscription_topics.reserve(stat_subscriptions_.size());
    for(const auto& [topic, sub_count] : stat_subscriptions_) {
        if(sub_count > 0) {
            subscription_topics.insert(topic);
        }
    }

    // Update subscriptions
    ManagerLocator::getMetricsManager().updateSubscriptions(global_subscription, std::move(subscription_topics));
}

void CMDPSink::enableSending(std::string sender_name) {
    sender_name_ = std::move(sender_name);

    // Get CMDP logger
    logger_ = std::make_unique<Logger>("LINK");

    // Start thread monitoring the socket for subscription messages
    subscription_thread_ = std::jthread(std::bind_front(&CMDPSink::subscription_loop, this));
    set_thread_name(subscription_thread_, "CMDPSink");

    // Register service in CHIRP
    auto* chirp_manager = ManagerLocator::getCHIRPManager();
    if(chirp_manager != nullptr) {
        chirp_manager->registerService(CHIRP::MONITORING, port_);
    } else {
        LOG(*logger_, WARNING) << "Failed to advertise logging on the network, satellite might not be discovered";
    }
    LOG(*logger_, INFO) << "Starting to log on port " << port_;
}

void CMDPSink::disableSending() {
    // Nothing to disable if sending was never enabled
    if(logger_ == nullptr) {
        return;
    }

    LOG(*logger_, DEBUG) << "Disabling logging via CMDP";

    subscription_thread_.request_stop();
    if(subscription_thread_.joinable()) {
        subscription_thread_.join();
    }

    auto* chirp_manager = ManagerLocator::getCHIRPManager();
    if(chirp_manager != nullptr) {
        chirp_manager->unregisterService(CHIRP::MONITORING, port_);
    }

    // Reset log levels
    log_subscriptions_.clear();
    ManagerLocator::getSinkManager().updateCMDPLevels(OFF);

    // Delete CDMP logger to avoid circular dependency on destruction of CMDPSink
    logger_.reset();
}

void CMDPSink::sink_it_(const spdlog::details::log_msg& msg) {
    // Create message header
    auto msghead = CMDP1Message::Header(sender_name_, msg.time);
    // Add source and thread information at DEBUG, TRACE and CRITICAL
    if(from_spdlog_level(msg.level) <= DEBUG || from_spdlog_level(msg.level) == CRITICAL) {
        msghead.setTag("thread", static_cast<std::int64_t>(msg.thread_id));
        // Add log source if not empty
        if(!msg.source.empty()) [[likely]] {
            msghead.setTag("filename", get_rel_file_path(msg.source.filename));
            msghead.setTag("lineno", static_cast<std::int64_t>(msg.source.line));
            msghead.setTag("funcName", std::string_view(msg.source.funcname));
        }
    }

    try {
        // Create and send CMDP message
        CMDP1LogMessage(from_spdlog_level(msg.level),
                        to_string(msg.logger_name), // NOLINT(misc-include-cleaner) might be fmt string
                        std::move(msghead),
                        to_string(msg.payload))
            .assemble()
            .send(pub_socket_);
    } catch(const zmq::error_t& e) {
        throw NetworkError(e.what());
    }
}

void CMDPSink::sinkMetric(MetricValue metric_value) {
    // Create message header
    auto msghead = CMDP1Message::Header(sender_name_, std::chrono::system_clock::now());

    // Lock the mutex - automatically done for regular logging:
    const std::lock_guard<std::mutex> lock {mutex_};

    try {
        // Create and send CMDP message
        CMDP1StatMessage(std::move(msghead), std::move(metric_value)).assemble().send(pub_socket_);
    } catch(const zmq::error_t& e) {
        throw NetworkError(e.what());
    }
}

void CMDPSink::sinkNotification(std::string id, Dictionary topics) {
    // Create message header
    auto msghead = CMDP1Message::Header(sender_name_, std::chrono::system_clock::now());

    // Lock the mutex - automatically done for regular logging:
    const std::lock_guard<std::mutex> lock {mutex_};

    try {
        // Create and send CMDP message
        CMDP1Notification(std::move(msghead), std::move(id), std::move(topics)).assemble().send(pub_socket_);
    } catch(const zmq::error_t& e) {
        throw NetworkError(e.what());
    }
}
