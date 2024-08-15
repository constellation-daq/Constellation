/**
 * @file
 * @brief Data receiving satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include <zmq.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/pools/BasePool.hpp"
#include "constellation/satellite/BaseSatellite.hpp"
#include "constellation/satellite/Satellite.hpp"

namespace constellation::satellite {
    /**
     * @brief Satellite class with additional functions to receive data
     */
    class CNSTLN_API ReceiverSatellite
        : public Satellite,
          private pools::BasePool<message::CDTP1Message, chirp::DATA, zmq::socket_type::pull> {
    protected:
        /**
         * @brief Construct a data receiving satellite
         *
         * @param type Satellite type
         * @param name Name of this satellite instance
         */
        ReceiverSatellite(std::string_view type, std::string_view name);

        /**
         * @brief Receive and handle Begin-of-Run (BOR) message
         *
         * @param header Header of the BOR message containing e.g. the sender name
         * @param config Configuration of the sending satellite to be stored
         */
        virtual void receive_bor(const message::CDTP1Message::Header& header, config::Configuration config) = 0;

        /**
         * @brief Receive and handle data message
         *
         * @note Any tags in the header should be stored next to the payload frames.
         *
         * @param data_message Data message containing the header and the payload
         */
        virtual void receive_data(const message::CDTP1Message& data_message) = 0;

        /**
         * @brief Receive and handle End-of-Run (EOR) message
         *
         * @param header Header of the EOR message containing e.g. the sender name
         * @param run_metadata Dictionary with run metata of the sending satellite to be stored
         */
        virtual void receive_eor(const message::CDTP1Message::Header& header, config::Dictionary run_metadata) = 0;

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
         * * `_data_transmitters`
         *
         * @param config Configuration of the satellite
         */
        void initializing_receiver(config::Configuration& config);

        /**
         * @brief Reconfigure receiver components of satellite
         *
         * Supports reconfiguring of the following config parameters:
         * * `_data_transmitters`
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
         * This function stops the BasePool thread.
         */
        void stopping_receiver();

        /**
         * @brief Callback function for BasePool to handle CDTP messages
         *
         * @param message Received CDTP message
         */
        void handle_cdtp_message(const message::CDTP1Message& message);

    private:
        log::Logger cdtp_logger_;
        std::vector<std::string> data_transmitters_;
    };

} // namespace constellation::satellite
