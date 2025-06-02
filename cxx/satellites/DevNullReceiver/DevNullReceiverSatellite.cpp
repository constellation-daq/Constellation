/**
 * @file
 * @brief Implementation of data receiving and discarding satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "DevNullReceiverSatellite.hpp"

#include <chrono>
#include <functional>
#include <string_view>

#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"

using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::protocol::CSCP;
using namespace constellation::satellite;

DevNullReceiverSatellite::DevNullReceiverSatellite(std::string_view type, std::string_view name)
    : ReceiverSatellite(type, name) {
    support_reconfigure();

    register_command("get_data_rate",
                     "Get data rate during the last run in Gbps",
                     {State::ORBIT},
                     std::function<double(void)>([&]() { return data_rate_.load(); }));
}

void DevNullReceiverSatellite::starting(std::string_view /* run_identifier */) {
    timer_.start();
}

void DevNullReceiverSatellite::stopping() {
    timer_.stop();

    const auto run_duration_ns = timer_.duration();
    const auto run_duration_s = std::chrono::duration_cast<std::chrono::seconds>(run_duration_ns);

    const auto bytes_received = get_bytes_received();
    const auto data_rate = 8 * static_cast<double>(bytes_received) / static_cast<double>(run_duration_ns.count());
    data_rate_.store(data_rate);

    LOG(STATUS) << "Received " << 1e-9 * static_cast<double>(bytes_received) << " GB in " << run_duration_s << " ("
                << data_rate << " Gbps)";
}

void DevNullReceiverSatellite::receive_bor(const CDTP1Message::Header& header, Configuration config) {
    LOG(INFO) << "Received BOR from " << header.getSender() << " with config" << config.getDictionary().to_string();
}

void DevNullReceiverSatellite::receive_data(CDTP1Message /*data_message*/) {
    // Drop message
}

void DevNullReceiverSatellite::receive_eor(const CDTP1Message::Header& header, Dictionary run_metadata) {
    LOG(INFO) << "Received EOR from " << header.getSender() << " with metadata" << run_metadata.to_string();
}
