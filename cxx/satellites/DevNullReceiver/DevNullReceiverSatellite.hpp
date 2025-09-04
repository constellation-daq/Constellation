/**
 * @file
 * @brief Data receiving and discarding satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CDTP2Message.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"

class DevNullReceiverSatellite final : public constellation::satellite::ReceiverSatellite {
public:
    DevNullReceiverSatellite(std::string_view type, std::string_view name);

    void starting(std::string_view run_identifier) final;
    void stopping() final;

protected:
    void receive_bor(std::string_view sender,
                     const constellation::config::Dictionary& user_tags,
                     const constellation::config::Configuration& config) final;
    void receive_data(std::string_view sender, const constellation::message::CDTP2Message::DataRecord& data_record) final;
    void receive_eor(std::string_view sender,
                     const constellation::config::Dictionary& user_tags,
                     const constellation::config::Dictionary& run_metadata) final;

private:
    constellation::utils::StopwatchTimer timer_;
    std::atomic<double> data_rate_;
};
