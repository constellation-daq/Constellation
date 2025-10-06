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
#include <functional>
#include <future>
#include <numeric>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

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
#include "constellation/core/utils/timers.hpp"
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

constexpr unsigned ATOMIC_QUEUE_DEFAULT_SIZE = 32768;

TransmitterSatellite::TransmitterSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), cdtp_push_socket_(*global_zmq_context(), zmq::socket_type::push),
      cdtp_port_(bind_ephemeral_port(cdtp_push_socket_)), cdtp_logger_("DATA"), data_queue_size_(ATOMIC_QUEUE_DEFAULT_SIZE),
      data_record_queue_(data_queue_size_) {

    register_timed_metric("TX_BYTES",
                          "B",
                          MetricType::LAST_VALUE,
                          "Number of bytes transmitted by this satellite in the current run",
                          10s,
                          {CSCP::State::RUN, CSCP::State::stopping, CSCP::State::interrupting},
                          [this]() { return bytes_transmitted_.load(); });
    register_timed_metric("TX_BLOCKS",
                          "",
                          MetricType::LAST_VALUE,
                          "Number of blocks transmitted by this satellite in the current run",
                          10s,
                          {CSCP::State::RUN, CSCP::State::stopping, CSCP::State::interrupting},
                          [this]() { return blocks_transmitted_.load(); });
    register_timed_metric("TX_RECORDS",
                          "",
                          MetricType::LAST_VALUE,
                          "Number of data records transmitted by this satellite in the current run",
                          10s,
                          {CSCP::State::RUN, CSCP::State::stopping, CSCP::State::interrupting},
                          [this]() { return data_records_transmitted_.load(); });

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

CDTP2Message::DataRecord TransmitterSatellite::newDataRecord(std::size_t blocks) {
    // Increase sequence counter and return new message
    return {++seq_, {}, blocks};
}

void TransmitterSatellite::initializing_transmitter(Configuration& config) {
    data_bor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_bor_timeout", 10));
    data_eor_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_eor_timeout", 10));
    data_msg_timeout_ = std::chrono::seconds(config.get<std::uint64_t>("_data_timeout", 10));
    LOG(cdtp_logger_, DEBUG) << "Timeout for BOR message " << data_bor_timeout_ << ", for EOR message " << data_eor_timeout_
                             << ", for DATA message " << data_msg_timeout_;

    data_payload_threshold_ = config.get<std::size_t>("_payload_threshold", 128);
    LOG(cdtp_logger_, DEBUG) << "Payload threshold for sending off data messages: " << data_payload_threshold_ << "KiB";
    data_queue_size_ = config.get<unsigned>("_queue_size", ATOMIC_QUEUE_DEFAULT_SIZE);
    data_record_queue_ = AtomicQueueT(data_queue_size_);
    LOG(cdtp_logger_, DEBUG) << "Queue size for data records: " << data_queue_size_;

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
        LOG(cdtp_logger_, DEBUG) << "Reconfigured payload threshold: " << data_payload_threshold_ << "KiB";
    }
    if(partial_config.has("_queue_size")) {
        data_queue_size_ = partial_config.get<unsigned>("_queue_size");
        data_record_queue_ = AtomicQueueT(data_queue_size_);
        LOG(cdtp_logger_, DEBUG) << "Reconfigured queue size for data records: " << data_queue_size_;
    }
    if(partial_config.has("_data_license")) {
        data_license_ = partial_config.get<std::string>("_data_license");
        LOG(cdtp_logger_, INFO) << "Data license updated to " << data_license_;
    }
}

void TransmitterSatellite::starting_transmitter(std::string_view run_identifier, const config::Configuration& config) {
    // Reset telemetry
    bytes_transmitted_ = 0;
    blocks_transmitted_ = 0;
    data_records_transmitted_ = 0;
    STAT("TX_BYTES", 0);
    STAT("TX_BLOCKS", 0);
    STAT("TX_RECORDS", 0);

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
    // Stop sending thread
    stop_sending_loop();
    // Send EOR
    const auto condition_code = append_run_conditions(CDTP::RunCondition::GOOD);
    set_run_metadata_tag("condition_code", condition_code);
    set_run_metadata_tag("condition", enum_name(condition_code));
    send_eor();
}

void TransmitterSatellite::interrupting_transmitter(CSCP::State previous_state) {
    // Stop sending thread
    stop_sending_loop();
    // If previous state was running, stop the run by sending an EOR
    if(previous_state == CSCP::State::RUN) {
        const auto condition_code = append_run_conditions(CDTP::RunCondition::INTERRUPTED);
        set_run_metadata_tag("condition_code", condition_code);
        set_run_metadata_tag("condition", enum_name(condition_code));
        send_eor();
    }
}

void TransmitterSatellite::failure_transmitter(CSCP::State previous_state) {
    // Stop sending thread
    stop_sending_loop();
    // If previous state was running, attempt to send an EOR
    if(previous_state == CSCP::State::RUN) {
        markRunTainted();
        const auto condition_code = append_run_conditions(CDTP::RunCondition::ABORTED);
        set_run_metadata_tag("condition_code", condition_code);
        set_run_metadata_tag("condition", enum_name(condition_code));
        send_eor();
    }
}

void TransmitterSatellite::stop_sending_loop() {
    // Wait until data record queue is empty if sending thread still running
    while(sending_thread_.joinable() && !data_record_queue_.was_empty()) {
        std::this_thread::yield();
    }
    // Stop sending thread and join
    sending_thread_.request_stop();
    if(sending_thread_.joinable()) {
        sending_thread_.join();
    }
    // Clear the queue (in case sending thread failed before the queue was empty)
    while(!data_record_queue_.was_empty()) {
        data_record_queue_.pop();
    }
}

void TransmitterSatellite::sending_loop(const std::stop_token& stop_token) {
    TimeoutTimer send_timer {100ms};
    std::size_t current_payload_bytes = 0;

    // Convert data payload threshold from KiB to bytes
    const auto data_payload_threshold_b = data_payload_threshold_ * 1024;

    // Preallocate message (assume worst case 8B scenario)
    const auto max_data_records = (data_payload_threshold_b / 8) + 1;
    auto message = CDTP2Message(getCanonicalName(), CDTP2Message::Type::DATA, max_data_records);

    // Note: stop_sending_loop ensure that queue is empty before stop_request is called
    while(!stop_token.stop_requested()) {
        // Try popping an element from the queue
        CDTP2Message::DataRecord data_record;
        const auto popped = data_record_queue_.try_pop(data_record);

        // If popped handle block, otherwise check for timeout
        if(popped) {
            current_payload_bytes += data_record.countPayloadBytes();
            message.addDataRecord(std::move(data_record));

            // If threshold not reached, continue
            if(current_payload_bytes < data_payload_threshold_b) {
                continue;
            }
        } else {
            // Skip if send timeout is not reached yet
            if(!send_timer.timeoutReached()) {
                std::this_thread::yield();
                continue;
            }
            // Skip if nothing to send even after timeout was reached
            if(current_payload_bytes == 0) {
                send_timer.reset();
                continue;
            }
        }

        // Send message
        const auto success = send_data(message, current_payload_bytes);
        if(!success) [[unlikely]] {
            return;
        }

        // Reset timer and counter
        send_timer.reset();
        current_payload_bytes = 0;
    }

    // Send remaining data blocks
    if(!message.getDataRecords().empty()) {
        send_data(message, current_payload_bytes);
    }
}

bool TransmitterSatellite::send_data(CDTP2Message& message, std::size_t current_payload_bytes) {
    // Log and update telemetry
    const auto& current_data_records = message.getDataRecords();
    LOG(cdtp_logger_, TRACE) << "Sending data records from " << current_data_records.front().getSequenceNumber() << " to "
                             << current_data_records.back().getSequenceNumber() << " (" << current_payload_bytes
                             << " bytes)";
    bytes_transmitted_ += current_payload_bytes;
    blocks_transmitted_ += std::transform_reduce(
        current_data_records.begin(), current_data_records.end(), 0UL, std::plus(), [](const auto& data_record) {
            return data_record.countBlocks();
        });
    data_records_transmitted_ += current_data_records.size();

    // Send message
    try {
        const auto sent = message.assemble().send(cdtp_push_socket_);
        if(!sent) [[unlikely]] {
            send_failure("data timeout reached");
            return false;
        }
    } catch(const zmq::error_t& error) {
        send_failure(error.what());
        return false;
    }

    // Clear blocks
    message.clearBlocks();

    return true;
}

void TransmitterSatellite::send_failure(const std::string& reason) {
    // Request failure as async future
    auto failure_fut =
        std::async(std::launch::async, [this, &reason]() { getFSM().requestFailure("Failed to send message: " + reason); });
    // While still in RUN state, pop queue to avoid deadlock with block queue push
    while(getState() == CSCP::State::RUN) {
        while(!data_record_queue_.was_empty()) {
            data_record_queue_.pop();
        }
        std::this_thread::yield();
    }
    // Join future
    failure_fut.get();
}
