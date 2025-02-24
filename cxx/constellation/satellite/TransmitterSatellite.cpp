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
#include <cstddef>
#include <cstdint>
#include <exception>
#include <stop_token>
#include <string_view>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/metrics/Metric.hpp"
#include "constellation/core/metrics/stat.hpp"
#include "constellation/core/networking/exceptions.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"
#include "constellation/core/protocol/CDTP_definitions.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"

#include "Satellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::metrics;
using namespace constellation::networking;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;
using namespace std::chrono_literals;

TransmitterSatellite::TransmitterSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), cdtp_push_socket_(*global_zmq_context(), zmq::socket_type::push),
      cdtp_port_(bind_ephemeral_port(cdtp_push_socket_)), cdtp_logger_("CDTP") {

    register_timed_metric("BYTES_TRANSMITTED",
                          "B",
                          MetricType::LAST_VALUE,
                          10s,
                          {CSCP::State::starting, CSCP::State::RUN, CSCP::State::stopping},
                          [this]() { return bytes_transmitted_.load(); });

    try {
        // Only send to completed connections
        cdtp_push_socket_.set(zmq::sockopt::immediate, true);
    } catch(const zmq::error_t& e) {
        throw NetworkError(e.what());
    }

    // Announce service via CHIRP
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
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

TransmitterSatellite::DataMessage TransmitterSatellite::newDataMessage(std::size_t frames) {
    // Increase sequence counter and return new message
    return {getCanonicalName(), ++seq_, frames};
}

void TransmitterSatellite::sendDataMessage(TransmitterSatellite::DataMessage&& message) {
    LOG(cdtp_logger_, TRACE) << "Queuing data message " << message.getHeader().getSequenceNumber();

    // Rethrow any exceptions from data queue worker
    if(data_exception_ != nullptr) {
        std::rethrow_exception(data_exception_);
    }

    // Add data message to queue
    std::unique_lock data_queue_lock {data_queue_mutex_};
    data_queue_.emplace_back(std::move(message));
    data_queue_lock.unlock();
    data_queue_cv_.notify_one();
}

void TransmitterSatellite::initializing_transmitter(Configuration& config) {
    data_bor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_bor_timeout", 10));
    data_eor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_eor_timeout", 10));
    data_msg_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_data_timeout", 10));
    LOG(cdtp_logger_, DEBUG) << "Timeout for BOR message " << data_bor_timeout_ << ", for EOR message " << data_eor_timeout_
                             << ", for DATA message " << data_msg_timeout_;
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
}

void TransmitterSatellite::starting_transmitter(std::string_view run_identifier, const config::Configuration& config) {
    // Reset bytes transmitted metric
    bytes_transmitted_ = 0;
    STAT("BYTES_TRANSMITTED", 0);

    // Reset run metadata and sequence counter
    seq_ = 0;
    run_metadata_ = {};
    mark_run_tainted_ = false;
    set_run_metadata_tag("version", CNSTLN_VERSION);
    set_run_metadata_tag("version_full", "Constellation " CNSTLN_VERSION_FULL);
    set_run_metadata_tag("run_id", run_identifier);
    set_run_metadata_tag("time_start", std::chrono::system_clock::now());

    // Create CDTP1 message for BOR
    CDTP1Message msg {{getCanonicalName(), seq_, CDTP1Message::Type::BOR, bor_tags_}, 1};
    msg.addPayload(config.getDictionary().assemble());

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

    // Reset data exception
    data_exception_ = nullptr;

    // Set timeout for data sending
    set_send_timeout(data_msg_timeout_);

    // Clear BOR tags:
    bor_tags_ = {};
}

void TransmitterSatellite::send_data(zmq::multipart_t& mme_cdtp_message_mp) {
    try {
        // Encode encoded CDTP messages again and send
        const auto send_result = cdtp_push_socket_.send(mme_cdtp_message_mp.encode(), zmq::send_flags::dontwait);
        if(send_result.has_value()) [[likely]] {
            LOG(cdtp_logger_, TRACE) << "Sent data messages with combined size of " << send_result.value() << " bytes";
            bytes_transmitted_ += send_result.value();
        } else {
            data_exception_ = std::make_exception_ptr(SendTimeoutError("data message", data_msg_timeout_));
        }
    } catch(const zmq::error_t& error) {
        data_exception_ = std::make_exception_ptr(NetworkError(error.what()));
    }

    // TODO check if this needs to be added: mme_cdtp_message_mp.clear();
}

void TransmitterSatellite::send_loop(const std::stop_token& stop_token) {
    auto last_send_time = std::chrono::steady_clock::now();
    std::size_t accumulated_bytes = 0;

    std::size_t cdtp_double_mme_threshold = 30000; // TODO: make configurable

    // Notify condition variable when stop is requested
    const std::stop_callback stop_callback {stop_token, [&]() { data_queue_cv_.notify_one(); }};

    // Create variables for multi-message encoding
    zmq::message_t mme_cdtp_message {};
    zmq::multipart_t mme_cdtp_message_mp {};

    while(!stop_token.stop_requested()) {
        std::unique_lock data_queue_lock {data_queue_mutex_};
        data_queue_cv_.wait(data_queue_lock);

        // Get and encode data messages, add to multipart message
        while(!data_queue_.empty()) {
            mme_cdtp_message = data_queue_.front().assemble().encode();
            data_queue_.pop_front();
            accumulated_bytes += mme_cdtp_message.size();
            mme_cdtp_message_mp.add(std::move(mme_cdtp_message));
        }
        data_queue_lock.unlock();

        // Check if threshold is reached or time limit exceeded
        if(accumulated_bytes >= cdtp_double_mme_threshold ||
           (!mme_cdtp_message_mp.empty() && last_send_time > std::chrono::steady_clock::now() - 500ms)) {
            // Send encoded data messages
            send_data(mme_cdtp_message_mp);

            // Reset accumulated bytes and last send time
            accumulated_bytes = 0;
            last_send_time = std::chrono::steady_clock::now();
        }
    }

    // Send any remaining data messages
    const std::lock_guard data_queue_lock {data_queue_mutex_};
    while(!data_queue_.empty()) {
        mme_cdtp_message_mp.add(data_queue_.front().assemble().encode());
        data_queue_.pop_front();
    }
    if(!mme_cdtp_message_mp.empty()) {
        send_data(mme_cdtp_message_mp);
    }
}

void TransmitterSatellite::send_eor() {
    set_run_metadata_tag("time_end", std::chrono::system_clock::now());

    // Create CDTP1 message for EOR
    CDTP1Message msg {{getCanonicalName(), ++seq_, CDTP1Message::Type::EOR, eor_tags_}, 1};
    msg.addPayload(run_metadata_.assemble());

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

    // Clear EOR tags:
    eor_tags_ = {};
}

void TransmitterSatellite::stopping_transmitter() {
    // Rethrow any exceptions from data queue worker
    if(data_exception_ != nullptr) {
        std::rethrow_exception(data_exception_);
    }

    if(mark_run_tainted_) {
        set_run_metadata_tag("condition_code", CDTP::RunCondition::TAINTED);
        set_run_metadata_tag("condition", enum_name(CDTP::RunCondition::TAINTED));
    } else {
        set_run_metadata_tag("condition_code", CDTP::RunCondition::GOOD);
        set_run_metadata_tag("condition", enum_name(CDTP::RunCondition::GOOD));
    }
    send_eor();
}

void TransmitterSatellite::interrupting_transmitter(CSCP::State previous_state) {
    // If previous state was running, stop the run by sending an EOR
    if(previous_state == CSCP::State::RUN) {
        // Rethrow any exceptions from data queue worker
        if(data_exception_ != nullptr) {
            std::rethrow_exception(data_exception_);
        }

        auto condition_code = CDTP::RunCondition::INTERRUPTED;
        if(mark_run_tainted_) {
            condition_code |= CDTP::RunCondition::TAINTED;
        }
        set_run_metadata_tag("condition_code", condition_code);
        set_run_metadata_tag("condition", enum_name(condition_code));
        send_eor();
    }
}
