/**
 * @file
 * @brief Satellite receiving data and storing it as EUDAQ RawData files
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CDTP2Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"

#include "FileSerializer.hpp"

class EudaqNativeWriterSatellite final : public constellation::satellite::ReceiverSatellite {
public:
    /** Satellite constructor */
    EudaqNativeWriterSatellite(std::string_view type, std::string_view name);

    /** Transition function for initialize command */
    void initializing(constellation::config::Configuration& config) final;

    /** Transition function for start command */
    void starting(std::string_view run_identifier) final;

    /** Transition function for stop command */
    void stopping() final;

    /** Transition function for interrupt transition to SAFE mode */
    void interrupting(constellation::protocol::CSCP::State previous_state, std::string_view reason) final;

    /** Transition function for failure transition to ERROR mode */
    void failure(constellation::protocol::CSCP::State previous_state, std::string_view reason) final;

protected:
    /** Callback for receiving a BOR message */
    void receive_bor(std::string_view sender,
                     const constellation::config::Dictionary& user_tags,
                     const constellation::config::Configuration& config) final;

    /** Callback for receiving data records in a DATA message */
    void receive_data(std::string_view sender, const constellation::message::CDTP2Message::DataRecord& data_record) final;

    /** Callback for receiving a EOR message */
    void receive_eor(std::string_view sender,
                     const constellation::config::Dictionary& user_tags,
                     const constellation::config::Dictionary& run_metadata) final;

private:
    std::unique_ptr<FileSerializer> serializer_;
    std::filesystem::path base_path_;
    constellation::utils::TimeoutTimer flush_timer_ {std::chrono::seconds(3)};
};
