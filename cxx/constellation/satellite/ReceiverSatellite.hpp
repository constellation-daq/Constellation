/**
 * @file
 * @brief Data receiving satellite
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
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CDTP2Message.hpp"
#include "constellation/core/pools/BasePool.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/string_hash_map.hpp"
#include "constellation/satellite/BaseSatellite.hpp"
#include "constellation/satellite/Satellite.hpp"

namespace constellation::satellite {
    /**
     * @brief Satellite class with additional functions to receive data
     */
    class CNSTLN_API ReceiverSatellite
        : public Satellite,
          private pools::BasePool<message::CDTP2Message, protocol::CHIRP::DATA, zmq::socket_type::pull> {
    private:
        using BasePoolT = BasePool<message::CDTP2Message, protocol::CHIRP::DATA, zmq::socket_type::pull>;

        enum class TransmitterState : std::uint8_t {
            NOT_CONNECTED,
            BOR_RECEIVED,
            EOR_RECEIVED,
        };
        struct TransmitterStateSeq {
            /** State of the CDTP connection */
            TransmitterState state;

            /** Sequence number of the last received message */
            std::uint64_t seq;

            /** Number of missed messages */
            std::uint64_t missed;
        };

    protected:
        /**
         * @brief Construct a data receiving satellite
         *
         * @param type Satellite type
         * @param name Name of this satellite instance
         */
        ReceiverSatellite(std::string_view type, std::string_view name);

        /**
         * @brief Validate the output directory
         * @details This method checks if the directory exists and is a directory, creates all parent directories if
         * necessary, and registers a disk space metric for the path.
         *
         * @param path Prospective output directory
         */
        void validate_output_directory(const std::filesystem::path& path);

        /**
         * @brief Create the final output file path from output directory, file name and extension and validates access
         *
         * @param path Output directory the file will be located in
         * @param file_name Name of the file
         * @param ext File extension of the file, will be only set if not empty
         *
         * @return Validated canonical path to the output file
         */
        std::filesystem::path validate_output_file(const std::filesystem::path& path,
                                                   const std::string& file_name,
                                                   const std::string& ext = "");

        /**
         * @brief Create the final output file from output directory, file name and extension
         *
         * @param path Output directory the file will be located in
         * @param file_name Name of the file
         * @param ext File extension of the file, will be only set if not empty
         * @param binary Boolean flag whether the file should be opened in binary mode or not
         *
         * @return Output file stream to the target file
         */
        std::ofstream create_output_file(const std::filesystem::path& path,
                                         const std::string& file_name,
                                         const std::string& ext = "",
                                         bool binary = true);

        /**
         * @brief Receive and handle Begin-of-Run (BOR) message
         *
         * @param sender Canonical name of the sending satellite
         * @param user_tags Dictionary with the user tags of the sending satellite
         * @param config Configuration of the sending satellite
         */
        virtual void receive_bor(std::string_view sender,
                                 const config::Dictionary& user_tags,
                                 const config::Configuration& config) = 0;

        /**
         * @brief Receive and handle data record
         *
         * @param sender Canonical name of the sending satellite
         * @param data_record Data record containing the user tags and payload
         */
        virtual void receive_data(std::string_view sender, const message::CDTP2Message::DataRecord& data_record) = 0;

        /**
         * @brief Receive and handle End-of-Run (EOR) message
         *
         * @param sender Canonical name of the sending satellite
         * @param user_tags Dictionary with the user tags of the sending satellite
         * @param run_metadata Dictionary with run meta data of the sending satellite
         */
        virtual void receive_eor(std::string_view sender,
                                 const config::Dictionary& user_tags,
                                 const config::Dictionary& run_metadata) = 0;

        /**
         * @brief Get amount of payload data received from all transmitters in the current run
         *
         * @return Amount of data in bytes
         */
        std::size_t get_bytes_received() const { return bytes_received_.load(); }

    public:
        /**
         * @brief Run function
         *
         * @note For data receiving satellites, this function must not be implemented. Instead `receive_bor()`,
                 `receive_data()` and `receive_eor()` have to be implemented.
         *
         * @param stop_token Token which tracks if running should be stopped or aborted
         */
        void running(const std::stop_token& stop_token) final;

    protected:
        /**
         * @brief Checks whether or not to connect to a discovered service
         *
         * @return True if service should be connected to, false otherwise
         */
        bool should_connect(const chirp::DiscoveredService& service) final;

    private:
        // Needs access to receiver specific functions
        friend BaseSatellite;

        /**
         * @brief Initialize receiver components of satellite
         *
         * Reads the following config parameters:
         * * `_allow_overwriting`
         * * `_data_transmitters`
         * * `_eor_timeout`
         *
         * @param config Configuration of the satellite
         */
        void initializing_receiver(config::Configuration& config);

        /**
         * @brief Reconfigure receiver components of satellite
         *
         * Supports reconfiguring of the following config parameters:
         * * `_allow_overwriting`
         * * `_eor_timeout`
         *
         * @param partial_config Changes to the configuration of the satellite
         */
        void reconfiguring_receiver(const config::Configuration& partial_config);

        /**
         * @brief Start receiver components of satellite
         *
         * This functions starts the BasePool thread to receive CDTP messages.
         */
        void starting_receiver();

        /**
         * @brief Stop receiver components of satellite
         *
         * This function stops the BasePool thread and waits for the EOR messages.
         *
         * @throw RecvTimeoutError If EOR of sallites that send a BOR is not received after timeout
         */
        void stopping_receiver();

        /**
         * @brief Interrupt receiver components of satellite
         *
         * This function calls the `stopping_receiver()` method, but emits fake EOR messages instead of throwing.
         */
        void interrupting_receiver(protocol::CSCP::State previous_state);

        /**
         * @brief Failure function for receiver components of satellite
         *
         * This function stops the BasePool thread.
         */
        void failure_receiver();

        /**
         * @brief Reset the states of all (configured) data transmitters to `NOT_CONNECTED`
         */
        void reset_data_transmitter_states();

        /**
         * @brief Callback function for BasePool to handle CDTP messages
         *
         * @param message Received CDTP message
         */
        void handle_cdtp_message(message::CDTP2Message&& message);

        /**
         * @brief Handle BOR message before passing it to `receive_bor()`
         *
         * @param bor_message Received CDTP BOR message
         * @throw InvalidCDTPMessageType If already a BOR received
         */
        void handle_bor_message(const message::CDTP2BORMessage& bor_message);

        /**
         * @brief Handle DATA message before passing it to `receive_data()`
         *
         * @param data_message Received CDTP DATA message
         * @throw InvalidCDTPMessageType If no BOR received yet
         */
        void handle_data_message(const message::CDTP2Message& data_message);

        /**
         * @brief Handle EOR message before passing it to `receive_eor()`
         *
         * @param eor_message Received CDTP EOR message
         * @throw InvalidCDTPMessageType If no BOR received yet
         */
        void handle_eor_message(const message::CDTP2EORMessage& eor_message);

        /**
         * @brief Register or update metrics for available disk space
         * @param path Path for which the available filesystem disk space should be evaluated
         */
        void register_diskspace_metric(const std::filesystem::path& path);

    private:
        std::chrono::seconds data_eor_timeout_ {};
        bool allow_overwriting_ {};
        std::set<std::string> data_transmitters_;
        utils::string_hash_map<TransmitterStateSeq> data_transmitter_states_;
        std::mutex data_transmitter_states_mutex_;
        std::atomic_size_t bytes_received_;
    };

} // namespace constellation::satellite
