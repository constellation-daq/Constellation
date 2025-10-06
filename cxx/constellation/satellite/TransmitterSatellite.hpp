/**
 * @file
 * @brief Data transmitting satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <atomic_queue/atomic_queue.h>
#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CDTP2Message.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/protocol/CDTP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/BaseSatellite.hpp"
#include "constellation/satellite/Satellite.hpp"

namespace constellation::satellite {
    /**
     * @brief Satellite class with additional functions to transmit data
     */
    class CNSTLN_API TransmitterSatellite : public Satellite {
    public:
        /**
         * @brief Create new data record
         *
         * @note This function increases the CDTP sequence number.
         * @note To send the data record, use `sendDataRecord()`.
         *
         * @param blocks Number of data record blocks to reserve
         */
        message::CDTP2Message::DataRecord newDataRecord(std::size_t blocks = 1);

        /**
         * @brief Queue data record for sending created with `newDataRecord()`
         *
         * @note This call might block if current data rate limited
         *
         * @param data_record Data record to send
         */
        void sendDataRecord(message::CDTP2Message::DataRecord&& data_record) {
            data_record_queue_.push(std::move(data_record));
        }

        /**
         * @brief Check if a data record can be send immediately
         *
         * @note If this functions returns false, the available data rate of the data transmission connection is too low for
         *       the rate at which the satellite is sending data.
         *
         * @return True if a data record send immediately, false otherwise
         */
        bool canSendRecord() const { return !data_record_queue_.was_full(); }

        /**
         * @brief Mark this run data as tainted
         * @details This will set the condition tag in the run metadata to `TAINTED` instead of `GOOD` to mark that there
         *          might be an issue with the data recorded during this run.
         */
        void markRunTainted() { mark_run_tainted_ = true; };

        /**
         * @brief Set tag for the BOR message metadata send at the begin of the run
         */
        template <typename T> void setBORTag(std::string_view key, const T& value) {
            bor_tags_[utils::transform(key, ::tolower)] = value;
        }

        /**
         * @brief Set tag for the EOR message metadata send at the end of the run
         */
        template <typename T> void setEORTag(std::string_view key, const T& value) {
            eor_tags_[utils::transform(key, ::tolower)] = value;
        }

        /**
         * @brief Return the ephemeral port number to which the CDTP socket is bound to
         */
        constexpr networking::Port getDataPort() const { return cdtp_port_; }

    protected:
        /**
         * @brief Construct a data transmitting satellite
         *
         * @param type Satellite type
         * @param name Name of this satellite instance
         */
        TransmitterSatellite(std::string_view type, std::string_view name);

    private:
        // Needs access to transmitter specific functions
        friend BaseSatellite;

        /**
         * @brief Initialize transmitter components of satellite
         *
         * Reads the following config parameters:
         * * `_bor_timeout`
         * * `_eor_timeout
         * * `_data_timeout`
         * * `_payload_threshold`
         * * `_queue_size`
         * * `_data_license`
         *
         * @param config Configuration of the satellite
         */
        void initializing_transmitter(config::Configuration& config);

        /**
         * @brief Reconfigure transmitter components of satellite
         *
         * Supports reconfiguring of the following config parameters:
         * * `_bor_timeout`
         * * `_eor_timeout`
         * * `_data_timeout`
         * * `_payload_threshold`
         * * `_queue_size`
         * * `_data_license`
         *
         * @param partial_config Changes to the configuration of the satellite
         */
        void reconfiguring_transmitter(const config::Configuration& partial_config);

        /**
         * @brief Start transmitter components of satellite
         *
         * This functions sends the BOR message.
         *
         * @param run_identifier Run identifier for the upcoming run
         * @param config Configuration to send in BOR message
         * @throw SendTimeoutError If BOR send timeout is reached
         */
        void starting_transmitter(std::string_view run_identifier, const config::Configuration& config);

        /**
         * @brief Stop transmitter components of satellite and send the EOR
         *
         * @throw SendTimeoutError If EOR send timeout is reached
         */
        void stopping_transmitter();

        /**
         * @brief Interrupt function of transmitter
         *
         * If the previous state is RUN, this sends an EOR message marking the end of the run indicating an interruption.
         *
         * @throw SendTimeoutError If EOR send timeout is reached
         *
         * @param previous_state State in which the satellite was being interrupted
         */
        void interrupting_transmitter(protocol::CSCP::State previous_state);

        /**
         * @brief Failure function of transmitter
         *
         * If the previous state is RUN, this marks the run as tainted and sends an EOR message marking the end of the run.
         *
         * @throw SendTimeoutError If EOR send timeout is reached
         *
         * @param previous_state State in which the satellite was being interrupted
         */
        void failure_transmitter(protocol::CSCP::State previous_state);

        /**
         * @brief Set send timeout
         *
         * @param timeout Timeout, -1 is infinite (block until sent)
         */
        CNSTLN_LOCAL void set_send_timeout(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1));

        /**
         * @brief Send the EOR message
         *
         * @throw SendTimeoutError If EOR send timeout is reached
         */
        CNSTLN_LOCAL void send_eor();

        /**
         * @brief Set tag for the run metadata send as payload of the EOR message
         */
        template <typename T> void set_run_metadata_tag(std::string_view key, const T& value) {
            run_metadata_[utils::transform(key, ::tolower)] = value;
        }

        /**
         * @brief Helper to append flags to the run condition
         *
         * @param conditions Input conditions
         * @return Amended conditions
         */
        CNSTLN_LOCAL protocol::CDTP::RunCondition append_run_conditions(protocol::CDTP::RunCondition conditions) const;

        /**
         * @brief Stop sending thread
         *
         * @note Requires running function to be already stopped such that no new data records are queued.
         */
        CNSTLN_LOCAL void stop_sending_loop();

        /**
         * @brief Sending loop sending data records from the queue
         *
         * @param stop_token Stop token
         */
        CNSTLN_LOCAL void sending_loop(const std::stop_token& stop_token);

        /**
         * @brief Send CDTP DATA message
         *
         * @param message Reference to data message
         * @param current_payload_bytes Size of the payload of the message in bytes
         * @return True if the message was sent successfully, false otherwise
         */
        CNSTLN_LOCAL bool send_data(message::CDTP2Message& message, std::size_t current_payload_bytes);

        /**
         * @brief Handle failure in `sending_loop`
         *
         * @param reason Reason for failure
         */
        CNSTLN_LOCAL void send_failure(const std::string& reason);

    private:
        zmq::socket_t cdtp_push_socket_;
        networking::Port cdtp_port_;
        log::Logger cdtp_logger_;

        std::chrono::seconds data_bor_timeout_ {};
        std::chrono::seconds data_eor_timeout_ {};
        std::chrono::seconds data_msg_timeout_ {};
        std::size_t data_payload_threshold_ {};
        unsigned data_queue_size_ {};

        // Atomic Queue Type: Maximize Throughput, Enable total order, disable Single-Producer-Single-Consumer
        using AtomicQueueT = atomic_queue::AtomicQueueB2<message::CDTP2Message::DataRecord,
                                                         std::allocator<message::CDTP2Message::DataRecord>,
                                                         true,
                                                         true,
                                                         false>;
        AtomicQueueT data_record_queue_;
        std::uint64_t seq_ {};
        std::jthread sending_thread_;

        config::Dictionary bor_tags_;
        config::Dictionary eor_tags_;
        config::Dictionary run_metadata_;
        std::string data_license_;
        bool mark_run_tainted_ {false};

        std::atomic_size_t bytes_transmitted_;
        std::atomic_size_t data_records_transmitted_;
        std::atomic_size_t blocks_transmitted_;
    };

} // namespace constellation::satellite
