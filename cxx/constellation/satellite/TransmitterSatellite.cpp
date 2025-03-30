/**
 * @file
 * @brief Implementation of a data transmitting satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "TransmitterSatellite.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <numeric>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP2Message.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/metrics/stat.hpp"
#include "constellation/core/networking/exceptions.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"
#include "constellation/core/protocol/CDTP_definitions.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::metrics;
using namespace constellation::networking;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;
using namespace std::string_literals;

TransmitterSatellite::TransmitterSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), cdtp_push_socket_(*global_zmq_context(), zmq::socket_type::push),
      cdtp_port_(bind_ephemeral_port(cdtp_push_socket_)), cdtp_logger_("DATA") {

    register_timed_metric("BYTES_TRANSMITTED",
                          "B",
                          MetricType::LAST_VALUE,
                          "Number of bytes transmitted by this satellite in the current run",
                          10s,
                          {CSCP::State::starting, CSCP::State::RUN, CSCP::State::stopping},
                          [this]() { return bytes_transmitted_.load(); });

    register_timed_metric("FRAMES_TRANSMITTED",
                          "",
                          MetricType::LAST_VALUE,
                          "Number of frames transmitted by this satellite over the current run",
                          3s,
                          {CSCP::State::starting, CSCP::State::RUN, CSCP::State::stopping},
                          [this]() { return frames_transmitted_.load(); });
    register_timed_metric("DATA_BLOCKS_TRANSMITTED",
                          "",
                          MetricType::LAST_VALUE,
                          "Number of data blocks transmitted by this satellite over the current run",
                          3s,
                          {CSCP::State::starting, CSCP::State::RUN, CSCP::State::stopping},
                          [this]() { return data_blocks_transmitted_.load(); });
    register_timed_metric("MESSAGES_TRANSMITTED",
                          "",
                          MetricType::LAST_VALUE,
                          "Number of messages transmitted by this satellite over the current run",
                          3s,
                          {CSCP::State::starting, CSCP::State::RUN, CSCP::State::stopping},
                          [this]() { return messages_transmitted_.load(); });

    try {
        // Only send to completed connections
        cdtp_push_socket_.set(zmq::sockopt::immediate, true);
    } catch(const zmq::error_t& e) {
        throw NetworkError(e.what());
    }

    // Announce service via CHIRP
    auto* chirp_manager = ManagerLocator::getCHIRPManager();
    if(chirp_manager != nullptr) {
        chirp_manager->registerService(CHIRP::DATA, cdtp_port_);
    }
    LOG(cdtp_logger_, INFO) << "Data will be sent on port " << cdtp_port_;
}

void TransmitterSatellite::set_send_timeout(std::chrono::milliseconds timeout) {
    try {
        cdtp_push_socket_.set(zmq::sockopt::sndtimeo, static_cast<int>(timeout.count()));
    } catch(const zmq::error_t& e) {
        throw NetworkError(e.what());
    }
}

CDTP2Message::DataBlock TransmitterSatellite::newDataBlock(std::size_t frames) {
    // Increase sequence counter and return new message
    return {++seq_, {}, frames};
}

void TransmitterSatellite::sendDataBlock(CDTP2Message::DataBlock&& data_block) {
    std::unique_lock data_block_queue_lock {data_block_queue_mutex_};
    data_block_queue_.emplace_back(std::move(data_block));
    data_block_queue_lock.unlock();
    data_block_queue_cv_.notify_one();
}

void TransmitterSatellite::initializing_transmitter(Configuration& config) {
    data_bor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_bor_timeout", 10));
    data_eor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_eor_timeout", 10));
    data_msg_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_data_timeout", 10));
    LOG(cdtp_logger_, DEBUG) << "Timeout for BOR message " << data_bor_timeout_ << ", for EOR message " << data_eor_timeout_
                             << ", for DATA message " << data_msg_timeout_;
    data_payload_threshold_ = config.get<std::size_t>("_payload_threshold", 32000);
    LOG(cdtp_logger_, DEBUG) << "Payload threshold for sending off data messages: " << data_payload_threshold_ << " bytes";

    data_license_ = config.get<std::string>("_data_license", "ODC-By-1.0");
    LOG(cdtp_logger_, INFO) << "Data will be stored under license " << data_license_;
}

void TransmitterSatellite::reconfiguring_transmitter(const Configuration& partial_config) {
    if(partial_config.has("_bor_timeout")) {
        data_bor_timeout_ = std::chrono::seconds(partial_config.get<std::uint64_t>("_bor_timeout"));
        LOG(cdtp_logger_, DEBUG) << "Reconfigured timeout for BOR message: " << data_bor_timeout_;
    }
    if(partial_config.has("_eor_timeout")) {
        data_eor_timeout_ = std::chrono::seconds(partial_config.get<std::uint64_t>("_eor_timeout"));
        LOG(cdtp_logger_, DEBUG) << "Reconfigured timeout for EOR message: " << data_eor_timeout_;
    }
    if(partial_config.has("_data_timeout")) {
        data_msg_timeout_ = std::chrono::seconds(partial_config.get<std::uint64_t>("_data_timeout"));
        LOG(cdtp_logger_, DEBUG) << "Reconfigured timeout for DATA message: " << data_msg_timeout_;
    }
    if(partial_config.has("_payload_threshold")) {
        data_payload_threshold_ = partial_config.get<std::size_t>("_payload_threshold");
        LOG(cdtp_logger_, DEBUG) << "Reconfigured payload threshold: " << data_payload_threshold_ << " bytes";
    }
    if(partial_config.has("_data_license")) {
        data_license_ = partial_config.get<std::string>("_data_license");
        LOG(cdtp_logger_, INFO) << "Data license updated to " << data_license_;
    }
}

void TransmitterSatellite::starting_transmitter(std::string_view run_identifier, const config::Configuration& config) {
    // Reset telemetry
    bytes_transmitted_ = 0;
    frames_transmitted_ = 0;
    data_blocks_transmitted_ = 0;
    messages_transmitted_ = 0;
    STAT("BYTES_TRANSMITTED", 0);
    STAT("FRAMES_TRANSMITTED", 0);
    STAT("DATA_BLOCKS_TRANSMITTED", 0);
    STAT("MESSAGES_TRANSMITTED", 0);

    // Reset run metadata and sequence counter
    seq_ = 0;
    run_metadata_ = {};
    mark_run_tainted_ = false;
    set_run_metadata_tag("version", CNSTLN_VERSION);
    set_run_metadata_tag("version_full", "Constellation " CNSTLN_VERSION_FULL);
    set_run_metadata_tag("run_id", run_identifier);
    set_run_metadata_tag("time_start", std::chrono::system_clock::now());
    set_run_metadata_tag("license", data_license_);

    // Create CDTP2 BOR message
    const CDTP2BORMessage msg {getCanonicalName(), std::move(bor_tags_), config};

    // Send BOR
    LOG(cdtp_logger_, DEBUG) << "Sending BOR message (timeout " << data_bor_timeout_ << ")";
    // Note: this is not interruptible, thus we set a send timeout to hang if no data receiver
    set_send_timeout(data_bor_timeout_);
    try {
        const auto sent = msg.assemble().send(cdtp_push_socket_);
        if(!sent) {
            throw SendTimeoutError("BOR message", data_bor_timeout_);
        }
    } catch(const zmq::error_t& e) {
        throw networking::NetworkError(e.what());
    }
    LOG(cdtp_logger_, DEBUG) << "Sent BOR message";

    // Set timeout for data sending
    set_send_timeout(data_msg_timeout_);

    // Start sending loop
    sending_thread_ = std::jthread(std::bind_front(&TransmitterSatellite::sending_loop, this));
}

void TransmitterSatellite::send_eor() {
    set_run_metadata_tag("time_end", std::chrono::system_clock::now());

    // Create CDTP2 EOR message
    const CDTP2EORMessage msg {getCanonicalName(), std::move(eor_tags_), std::move(run_metadata_)};

    // Send EOR
    LOG(cdtp_logger_, DEBUG) << "Sending EOR message (" << data_eor_timeout_ << ")";
    // Note: this is not interruptible, thus we set a send timeout to prevent hang if no data receiver
    set_send_timeout(data_eor_timeout_);
    try {
        const auto sent = msg.assemble().send(cdtp_push_socket_);
        if(!sent) {
            throw SendTimeoutError("EOR message", data_eor_timeout_);
        }
    } catch(const zmq::error_t& e) {
        throw networking::NetworkError(e.what());
    }
    LOG(cdtp_logger_, DEBUG) << "Sent EOR message";
}

CDTP::RunCondition TransmitterSatellite::append_run_conditions(CDTP::RunCondition conditions) const {
    if(mark_run_tainted_) {
        conditions |= CDTP::RunCondition::TAINTED;
    }
    if(is_run_degraded()) {
        conditions |= CDTP::RunCondition::DEGRADED;
    }
    return conditions;
}

void TransmitterSatellite::stopping_transmitter() {
    // Join sending thread
    sending_thread_.request_stop();
    if(sending_thread_.joinable()) {
        sending_thread_.join();
    }
    // Send EOR
    const auto conditions = append_run_conditions(CDTP::RunCondition::GOOD);
    set_run_metadata_tag("condition_code", conditions);
    set_run_metadata_tag("condition", enum_name(conditions));
    send_eor();
}

void TransmitterSatellite::interrupting_transmitter(CSCP::State previous_state) {
    // Join sending thread
    sending_thread_.request_stop();
    if(sending_thread_.joinable()) {
        sending_thread_.join();
    }
    // If previous state was running, stop the run by sending an EOR
    if(previous_state == CSCP::State::RUN) {
        auto condition_code = append_run_conditions(CDTP::RunCondition::INTERRUPTED);
        set_run_metadata_tag("condition_code", condition_code);
        set_run_metadata_tag("condition", enum_name(condition_code));
        send_eor();
    }
}

void TransmitterSatellite::failure_transmitter(CSCP::State previous_state) {
    // Join sending thread
    sending_thread_.request_stop();
    if(sending_thread_.joinable()) {
        sending_thread_.join();
    }
    // If previous state was running, attempt to send an EOR
    if(previous_state == CSCP::State::RUN) {
        const auto condition_code = CDTP::RunCondition::TAINTED;
        set_run_metadata_tag("condition_code", condition_code);
        set_run_metadata_tag("condition", enum_name(condition_code));
        send_eor();
    }
}

void TransmitterSatellite::sending_loop(const std::stop_token& stop_token) {
    // Notify condition variable when stop is requested
    const std::stop_callback stop_callback {stop_token, [&]() { data_block_queue_cv_.notify_all(); }};

    std::size_t current_payload_bytes = 0;
    std::vector<CDTP2Message::DataBlock> current_data_blocks {};

    // Always run loop at least once in case of early stop NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
    do {
        // Wait until new message is queued or some time has passed
        std::unique_lock data_block_queue_lock {data_block_queue_mutex_};
        const auto cv_status = data_block_queue_cv_.wait_for(data_block_queue_lock, 100ms);

        // Pop all message from queue
        while(!data_block_queue_.empty()) {
            current_payload_bytes += data_block_queue_.front().countPayloadBytes();
            current_data_blocks.emplace_back(std::move(data_block_queue_.front()));
            data_block_queue_.pop_front();
        }
        data_block_queue_lock.unlock();

        // If message was queued and stop not requested, check if data threshold is met
        if(cv_status == std::cv_status::no_timeout && !stop_token.stop_requested()) [[likely]] {
            if(current_payload_bytes < data_payload_threshold_) {
                continue;
            }
        }
        // Skip if nothing to send
        if(current_data_blocks.empty()) [[unlikely]] {
            continue;
        }

        // Log and update telemetry
        LOG(cdtp_logger_, TRACE) << "Sending data blocks from " << current_data_blocks.front().getSequenceNumber() << " to "
                                 << current_data_blocks.back().getSequenceNumber() << " (" << current_payload_bytes
                                 << " bytes)";
        bytes_transmitted_ += current_payload_bytes;
        frames_transmitted_ += std::transform_reduce(
            current_data_blocks.begin(), current_data_blocks.end(), 0UL, std::plus(), [](const auto& data_block) {
                return data_block.getFrames().size();
            });
        data_blocks_transmitted_ += current_data_blocks.size();
        ++messages_transmitted_;

        // Build message
        auto message = CDTP2Message(getCanonicalName(), CDTP2Message::Type::DATA);
        for(auto& data_block : current_data_blocks) {
            message.addDataBlock(std::move(data_block));
        }

        // Send message
        try {
            message.assemble().send(cdtp_push_socket_, static_cast<int>(zmq::send_flags::dontwait));
        } catch(const zmq::error_t& error) {
            getFSM().requestFailure("Failed to send message: "s + error.what());
            break;
        }

        // Reset message counters
        current_data_blocks.clear();
        current_payload_bytes = 0;

    } while(!stop_token.stop_requested());
}
