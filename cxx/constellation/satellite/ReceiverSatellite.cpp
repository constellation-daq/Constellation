/**
 * @file
 * @brief Implementation of a data receiving satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "ReceiverSatellite.hpp"

#include <algorithm>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/pools/BasePool.hpp"
#include "constellation/core/utils/std_future.hpp" // IWYU pragma: keep
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation;
using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::pools;
using namespace constellation::satellite;
using namespace constellation::utils;

ReceiverSatellite::ReceiverSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), BasePool("CDTP", [this](const CDTP1Message& message) { this->handle_cdtp_message(message); }),
      cdtp_logger_("CDTP") {}

void ReceiverSatellite::running(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        // Check and rethrow exception from BasePool
        checkPoolException();
    }
}

bool ReceiverSatellite::should_connect(const chirp::DiscoveredService& service) {
    return std::ranges::any_of(data_transmitters_,
                               [=](const auto& data_tramsitter) { return service.host_id == MD5Hash(data_tramsitter); });
}

void ReceiverSatellite::initializing_receiver(Configuration& config) {
    data_transmitters_ = config.getArray<std::string>("_data_transmitters");
    LOG(cdtp_logger_, INFO) << "Initialized to receive data from " << range_to_string(data_transmitters_);
}

void ReceiverSatellite::reconfiguring_receiver(const Configuration& partial_config) {
    if(partial_config.has("_data_transmitters")) {
        // BasePool disconnect all sockets when stopped, so this is safe to do
        data_transmitters_ = partial_config.getArray<std::string>("_data_transmitters");
        LOG(cdtp_logger_, INFO) << "Reconfigured to receive data from " << range_to_string(data_transmitters_);
    }
}

void ReceiverSatellite::starting_receiver() {
    // Request DATA services via CHIRP in case we missed something
    auto* chirp_manager = chirp::Manager::getDefaultInstance();
    if(chirp_manager != nullptr) {
        chirp_manager->sendRequest(chirp::DATA);
    }

    // Start BasePool thread
    startPool();
}

void ReceiverSatellite::stopping_receiver() {
    // Stop BasePool thread and disconnect all connected sockets
    stopPool();
}

void ReceiverSatellite::handle_cdtp_message(const CDTP1Message& message) {
    using enum CDTP1Message::Type;
    // TODO: fix logic by checking state for each sending satellite
    switch(message.getHeader().getType()) {
    case BOR: {
        LOG(cdtp_logger_, DEBUG) << "Received BOR message from " << message.getHeader().getSender();
        receive_bor(message.getHeader(), {Dictionary::disassemble(message.getPayload().at(0)), true});
        break;
    }
    case DATA: {
        receive_data(message);
        break;
    }
    case EOR: {
        LOG(cdtp_logger_, DEBUG) << "Received EOR message from " << message.getHeader().getSender();
        receive_eor(message.getHeader(), Dictionary::disassemble(message.getPayload().at(0)));
        break;
    }
    default: std::unreachable();
    }
}
