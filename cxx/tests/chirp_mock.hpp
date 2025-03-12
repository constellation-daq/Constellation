/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>

#include "constellation/core/chirp/BroadcastSend.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/utils/ManagerRegistry.hpp"

inline constellation::chirp::Manager* create_chirp_manager() {
    // TODO(stephan.lachnit): CHIRP manager should be part of registry,
    //                        for destruction order reasons needs to be created afterwards ManagerRegistry
    constellation::utils::ManagerRegistry::getInstance();
    static std::once_flag manager_flag {};
    std::call_once(manager_flag, [&] {
        LOG(STATUS) << "Creating chirp manager";
        auto manager = std::make_unique<constellation::chirp::Manager>("0.0.0.0", "0.0.0.0", "edda", "chirp_manager");
        manager->start();
        constellation::utils::ManagerRegistry::setDefaultCHIRPManager(std::move(manager));
    });
    return constellation::utils::ManagerRegistry::getCHIRPManager();
}

inline void chirp_mock_service(std::string_view name,
                               constellation::protocol::CHIRP::ServiceIdentifier service,
                               constellation::networking::Port port,
                               bool offer = true) {
    // Hack: add fake satellite to chirp to find satellite (cannot find from same manager)
    using namespace constellation::chirp;
    using namespace constellation::protocol::CHIRP;
    BroadcastSend chirp_sender {"0.0.0.0", PORT};
    const auto msgtype = offer ? MessageType::OFFER : MessageType::DEPART;
    const auto chirp_msg = constellation::message::CHIRPMessage(msgtype, "edda", name, service, port);
    chirp_sender.sendBroadcast(chirp_msg.assemble());
    // Wait until broadcast is received
    auto* manager = constellation::utils::ManagerRegistry::getCHIRPManager();
    while(std::ranges::count(manager->getDiscoveredServices(),
                             std::make_tuple(constellation::message::MD5Hash(name), service, port),
                             [](const auto& discovered_service) {
                                 return std::make_tuple(
                                     discovered_service.host_id, discovered_service.identifier, discovered_service.port);
                             }) == (msgtype == MessageType::OFFER ? 0 : 1)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class MockedChirpService {
public:
    MockedChirpService(std::string_view name,
                       constellation::protocol::CHIRP::ServiceIdentifier service,
                       constellation::networking::Port port)
        : name_(name), service_(service), port_(port) {
        chirp_mock_service(name, service, port, true);
    }

    ~MockedChirpService() { chirp_mock_service(name_, service_, port_, false); }

private:
    std::string name_;
    constellation::protocol::CHIRP::ServiceIdentifier service_;
    constellation::networking::Port port_;
};
