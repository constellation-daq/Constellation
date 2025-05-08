/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CDTP1Message.hpp"
#include "constellation/core/networking/exceptions.hpp"
#include "constellation/core/protocol/CDTP_definitions.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/satellite/FSM.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

#include "chirp_mock.hpp"
#include "dummy_satellite.hpp"

using namespace Catch::Matchers;
using namespace constellation;
using namespace constellation::config;
using namespace constellation::message;
using namespace constellation::networking;
using namespace constellation::protocol;
using namespace constellation::satellite;
using namespace constellation::utils;

class Receiver : public DummySatelliteNR<ReceiverSatellite> {
public:
    Receiver(std::string_view name = "r1") : DummySatelliteNR<ReceiverSatellite>(name) {}

protected:
    void receive_bor(const CDTP1Message::Header& header, Configuration config) override {
        const auto sender = to_string(header.getSender());
        const std::lock_guard map_lock {map_mutex_};
        bor_map_.erase(sender);
        bor_map_.emplace(sender, std::move(config));
        bor_tag_map_.erase(sender);
        bor_tag_map_.emplace(sender, header.getTags());
        bor_received_ = true;
    }
    void receive_data(CDTP1Message data_message) override {
        const auto sender = to_string(data_message.getHeader().getSender());
        const std::lock_guard map_lock {map_mutex_};
        last_data_map_.erase(sender);
        last_data_map_.emplace(sender, std::move(data_message));
        data_received_ = true;
    }
    void receive_eor(const CDTP1Message::Header& header, Dictionary run_metadata) override {
        const auto sender = to_string(header.getSender());
        const std::lock_guard map_lock {map_mutex_};
        eor_map_.erase(sender);
        eor_map_.emplace(sender, std::move(run_metadata));
        eor_tag_map_.erase(sender);
        eor_tag_map_.emplace(sender, header.getTags());
        eor_received_ = true;
    }

public:
    void awaitBOR() {
        while(!bor_received_.load()) {
            std::this_thread::yield();
        }
        bor_received_.store(false);
    }

    void awaitData() {
        while(!data_received_.load()) {
            std::this_thread::yield();
        }
        data_received_.store(false);
    }

    void awaitEOR() {
        while(!eor_received_.load()) {
            std::this_thread::yield();
        }
        eor_received_.store(false);
    }

    const Configuration& getBOR(const std::string& sender) {
        const std::lock_guard map_lock {map_mutex_};
        return bor_map_.at(sender);
    }
    const Dictionary& getBORTags(const std::string& sender) {
        const std::lock_guard map_lock {map_mutex_};
        return bor_tag_map_.at(sender);
    }
    const CDTP1Message& getLastData(const std::string& sender) {
        const std::lock_guard map_lock {map_mutex_};
        return last_data_map_.at(sender);
    }
    const Dictionary& getEOR(const std::string& sender) {
        const std::lock_guard map_lock {map_mutex_};
        return eor_map_.at(sender);
    }
    const Dictionary& getEORTags(const std::string& sender) {
        const std::lock_guard map_lock {map_mutex_};
        return eor_tag_map_.at(sender);
    }

private:
    std::mutex map_mutex_;
    std::atomic_bool bor_received_ {false};
    std::atomic_bool data_received_ {false};
    std::atomic_bool eor_received_ {false};
    std::map<std::string, Configuration> bor_map_;
    std::map<std::string, Dictionary> bor_tag_map_;
    std::map<std::string, CDTP1Message> last_data_map_;
    std::map<std::string, Dictionary> eor_map_;
    std::map<std::string, Dictionary> eor_tag_map_;
};

class Transmitter : public DummySatellite<TransmitterSatellite> {
public:
    Transmitter(std::string_view name = "t1") : DummySatellite<TransmitterSatellite>(name) {}

    template <typename T> bool trySendData(T data) {
        auto msg = newDataMessage();
        msg.addFrame(std::move(data));
        msg.addTag("test", 1);
        return trySendDataMessage(msg);
    }

    template <typename T> void sendData(T data) {
        auto msg = newDataMessage();
        msg.addFrame(std::move(data));
        msg.addTag("test", 1);
        sendDataMessage(msg);
    }
};

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Receiver / Reconfigure transmitters", "[satellite]") {
    auto receiver = Receiver();
    auto config = Configuration();
    config.set("_eor_timeout", 1);
    config.set("_allow_overwriting", true);
    receiver.reactFSM(FSM::Transition::initialize, std::move(config));
    receiver.reactFSM(FSM::Transition::launch);
    REQUIRE(receiver.getState() == FSM::State::ORBIT);
    auto config2 = Configuration();
    config2.setArray<std::string>("_data_transmitters", {"Dummy.t1"});
    receiver.reactFSM(FSM::Transition::reconfigure, std::move(config2));
    REQUIRE(receiver.getState() == FSM::State::ERROR);

    receiver.exit();
}

TEST_CASE("Receiver / Invalid transmitter name", "[satellite]") {
    auto receiver = Receiver();
    // Additional dot
    auto config1 = Configuration();
    config1.setArray<std::string>("_data_transmitters", {"satellites.Dummy.t1"});
    receiver.reactFSM(FSM::Transition::initialize, std::move(config1));
    REQUIRE(receiver.getState() == FSM::State::ERROR);
    // Missing dot
    auto config2 = Configuration();
    config2.setArray<std::string>("_data_transmitters", {"t1"});
    receiver.reactFSM(FSM::Transition::initialize, std::move(config2));
    REQUIRE(receiver.getState() == FSM::State::ERROR);
    // Invalid symbol
    auto config3 = Configuration();
    config3.setArray<std::string>("_data_transmitters", {"Dummy.t-1"});
    receiver.reactFSM(FSM::Transition::initialize, std::move(config3));
    REQUIRE(receiver.getState() == FSM::State::ERROR);

    receiver.exit();
}

TEST_CASE("Transmitter / BOR timeout", "[satellite]") {
    auto transmitter = Transmitter();
    auto config = Configuration();
    config.set("_bor_timeout", 1);
    config.set("_eor_timeout", 1);
    transmitter.reactFSM(FSM::Transition::initialize, std::move(config));
    transmitter.reactFSM(FSM::Transition::launch);
    transmitter.reactFSM(FSM::Transition::start, "test");
    // Require that transmitter went to error state due to BOR timeout
    REQUIRE(transmitter.getState() == FSM::State::ERROR);

    transmitter.exit();
}

TEST_CASE("Transmitter / DATA timeout", "[satellite]") {
    // Create CHIRP manager for data service discovery
    create_chirp_manager();

    auto transmitter = Transmitter();
    transmitter.mockChirpService(CHIRP::DATA);

    auto receiver = Receiver();
    auto config_receiver = Configuration();
    config_receiver.set("_eor_timeout", 1);
    config_receiver.setArray<std::string>("_data_transmitters", {transmitter.getCanonicalName()});

    auto config_transmitter = Configuration();
    config_transmitter.set("_data_timeout", 1);
    config_transmitter.set("_eor_timeout", 1);

    receiver.reactFSM(FSM::Transition::initialize, std::move(config_receiver));
    transmitter.reactFSM(FSM::Transition::initialize, std::move(config_transmitter));
    receiver.reactFSM(FSM::Transition::launch);
    transmitter.reactFSM(FSM::Transition::launch);
    receiver.reactFSM(FSM::Transition::start, "test");
    transmitter.reactFSM(FSM::Transition::start, "test");

    // Wait a bit for BOR to be handled by receiver
    receiver.awaitBOR();
    REQUIRE(receiver.getBOR(transmitter.getCanonicalName()).get<int>("_bor_timeout") == 10);

    // Stop the receiver to avoid receiving data
    receiver.reactFSM(FSM::Transition::stop);

    // Check that receiver went to ERROR due to missing EOR
    REQUIRE(receiver.getState() == FSM::State::ERROR);
    const auto& eor = receiver.getEOR(transmitter.getCanonicalName());
    REQUIRE(eor.at("condition").get<std::string>() == "ABORTED");
    REQUIRE(eor.at("condition_code").get<CDTP::RunCondition>() == CDTP::RunCondition::ABORTED);
    receiver.exit();

    // Attempt to send a data frame and catch its failure
    REQUIRE_THROWS_MATCHES(transmitter.sendData(std::vector<int>({1, 2, 3, 4})),
                           SendTimeoutError,
                           Message("Failed sending data message after 1s"));

    transmitter.exit();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Successful run", "[satellite]") {
    // Create CHIRP manager for data service discovery
    create_chirp_manager();

    auto receiver = Receiver();
    auto transmitter = Transmitter();
    transmitter.mockChirpService(CHIRP::DATA);

    auto config_receiver = Configuration();
    config_receiver.setArray<std::string>("_data_transmitters", {transmitter.getCanonicalName()});

    receiver.reactFSM(FSM::Transition::initialize, std::move(config_receiver));
    transmitter.reactFSM(FSM::Transition::initialize, Configuration());
    receiver.reactFSM(FSM::Transition::launch);
    transmitter.reactFSM(FSM::Transition::launch);

    auto config2_receiver = Configuration();
    config2_receiver.set("_allow_overwriting", true);
    config2_receiver.set("_eor_timeout", 1);
    auto config2_transmitter = Configuration();
    config2_transmitter.set("_bor_timeout", 1);
    config2_transmitter.set("_eor_timeout", 1);
    config2_transmitter.set("_data_timeout", 1);
    config2_transmitter.set("_data_license", "PDDL-1.0");
    receiver.reactFSM(FSM::Transition::reconfigure, std::move(config2_receiver));
    transmitter.reactFSM(FSM::Transition::reconfigure, std::move(config2_transmitter));

    // Set a tag for BOR
    transmitter.setBORTag("firmware_version", 3);

    receiver.reactFSM(FSM::Transition::start, "test");
    transmitter.reactFSM(FSM::Transition::start, "test");

    // Wait a bit for BOR to be handled by receiver
    receiver.awaitBOR();
    REQUIRE(receiver.getBOR(transmitter.getCanonicalName()).get<int>("_bor_timeout") == 1);

    const auto& bor_tags = receiver.getBORTags(transmitter.getCanonicalName());
    REQUIRE(bor_tags.at("firmware_version").get<int>() == 3);

    // Send a data frame
    const auto sent = transmitter.trySendData(std::vector<int>({1, 2, 3, 4}));
    REQUIRE(sent);
    // Wait a bit for data to be handled by receiver
    receiver.awaitData();
    const auto& data_msg = receiver.getLastData(transmitter.getCanonicalName());
    REQUIRE(data_msg.countPayloadFrames() == 1);
    REQUIRE(data_msg.getHeader().getTag<int>("test") == 1);

    // Set a tag for EOR
    transmitter.setEORTag("buggy_events", 10);

    // Stop and send EOR
    receiver.reactFSM(FSM::Transition::stop, {}, false);
    transmitter.reactFSM(FSM::Transition::stop);
    receiver.progressFsm();
    // Wait until EOR is handled
    receiver.awaitEOR();
    const auto& eor = receiver.getEOR(transmitter.getCanonicalName());
    REQUIRE(eor.at("version").get<std::string>() == CNSTLN_VERSION);
    REQUIRE(eor.at("version_full").get<std::string>() == "Constellation " CNSTLN_VERSION_FULL);
    REQUIRE(eor.at("run_id").get<std::string>() == "test");
    REQUIRE(eor.at("condition").get<std::string>() == "GOOD");
    REQUIRE(eor.at("condition_code").get<CDTP::RunCondition>() == CDTP::RunCondition::GOOD);
    REQUIRE(eor.at("license").get<std::string>() == "PDDL-1.0");

    const auto& eor_tags = receiver.getEORTags(transmitter.getCanonicalName());
    REQUIRE(eor_tags.at("buggy_events").get<int>() == 10);

    // Ensure all satellite are happy
    REQUIRE(receiver.getState() == FSM::State::ORBIT);
    REQUIRE(transmitter.getState() == FSM::State::ORBIT);

    receiver.exit();
    transmitter.exit();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Tainted run", "[satellite]") {
    // Create CHIRP manager for data service discovery
    create_chirp_manager();

    auto receiver = Receiver();
    auto transmitter = Transmitter();
    transmitter.mockChirpService(CHIRP::DATA);

    auto config_receiver = Configuration();
    config_receiver.set("_eor_timeout", 1);

    auto config_transmitter = Configuration();
    config_transmitter.set("_bor_timeout", 1);
    config_transmitter.set("_eor_timeout", 1);

    receiver.reactFSM(FSM::Transition::initialize, std::move(config_receiver));
    transmitter.reactFSM(FSM::Transition::initialize, std::move(config_transmitter));
    receiver.reactFSM(FSM::Transition::launch);
    transmitter.reactFSM(FSM::Transition::launch);
    receiver.reactFSM(FSM::Transition::start, "test");
    transmitter.reactFSM(FSM::Transition::start, "test");

    // Wait a bit for BOR to be handled by receiver
    receiver.awaitBOR();
    REQUIRE(receiver.getBOR(transmitter.getCanonicalName()).get<int>("_bor_timeout") == 1);

    // Send a data frame
    const auto sent = transmitter.trySendData(std::vector<int>({1, 2, 3, 4}));
    REQUIRE(sent);
    // Wait a bit for data to be handled by receiver
    receiver.awaitData();
    const auto& data_msg = receiver.getLastData(transmitter.getCanonicalName());
    REQUIRE(data_msg.countPayloadFrames() == 1);
    REQUIRE(data_msg.getHeader().getTag<int>("test") == 1);

    // Mark run as tainted:
    transmitter.markRunTainted();

    // Stop and send EOR
    receiver.reactFSM(FSM::Transition::stop, {}, false);
    transmitter.reactFSM(FSM::Transition::stop);
    // Wait until EOR is handled
    receiver.progressFsm();
    const auto& eor = receiver.getEOR(transmitter.getCanonicalName());
    REQUIRE(eor.at("run_id").get<std::string>() == "test");
    REQUIRE(eor.at("condition").get<std::string>() == "TAINTED");
    REQUIRE(eor.at("condition_code").get<CDTP::RunCondition>() == CDTP::RunCondition::TAINTED);

    // Ensure all satellite are happy
    REQUIRE(receiver.getState() == FSM::State::ORBIT);
    REQUIRE(transmitter.getState() == FSM::State::ORBIT);

    receiver.exit();
    transmitter.exit();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Transmitter interrupted run", "[satellite]") {
    // Create CHIRP manager for data service discovery
    create_chirp_manager();

    auto receiver = Receiver();
    auto transmitter = Transmitter();
    transmitter.mockChirpService(CHIRP::DATA);
    transmitter.mockChirpService(CHIRP::HEARTBEAT);

    auto config_receiver = Configuration();
    config_receiver.set("_eor_timeout", 1);
    config_receiver.setArray<std::string>("_data_transmitters", {transmitter.getCanonicalName()});

    auto config_transmitter = Configuration();
    config_transmitter.set("_bor_timeout", 1);
    config_transmitter.set("_eor_timeout", 1);

    receiver.reactFSM(FSM::Transition::initialize, std::move(config_receiver));
    transmitter.reactFSM(FSM::Transition::initialize, std::move(config_transmitter));
    receiver.reactFSM(FSM::Transition::launch);
    transmitter.reactFSM(FSM::Transition::launch);
    receiver.reactFSM(FSM::Transition::start, "test");
    transmitter.reactFSM(FSM::Transition::start, "test");

    // Wait a bit for BOR to be handled by receiver
    receiver.awaitBOR();
    REQUIRE(receiver.getBOR(transmitter.getCanonicalName()).get<int>("_bor_timeout") == 1);

    // Allow to progress through transitional state autonomously
    transmitter.skipTransitional(true);
    receiver.skipTransitional(true);

    // Interrupt the run
    transmitter.markRunTainted();
    transmitter.reactFSM(FSM::Transition::interrupt);

    // Wait until EOR is handled
    receiver.awaitEOR();
    const auto& eor = receiver.getEOR(transmitter.getCanonicalName());
    REQUIRE(eor.at("run_id").get<std::string>() == "test");
    REQUIRE(eor.at("condition").get<std::string>() == "TAINTED|INTERRUPTED");
    REQUIRE(eor.at("condition_code").get<CDTP::RunCondition>() ==
            (CDTP::RunCondition::TAINTED | CDTP::RunCondition::INTERRUPTED));

    // Ensure all transmitter is in safe mode and receiver is in interrupting or SAFE
    REQUIRE(transmitter.getState() == FSM::State::SAFE);
    const auto receiver_state = receiver.getState();
    REQUIRE((receiver_state == FSM::State::interrupting || receiver_state == FSM::State::SAFE));

    receiver.exit();
    transmitter.exit();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

TEST_CASE("Transmitter failure run", "[satellite]") {
    // Create CHIRP manager for data service discovery
    create_chirp_manager();

    auto receiver = Receiver();
    auto transmitter = Transmitter();
    transmitter.mockChirpService(CHIRP::DATA);
    transmitter.mockChirpService(CHIRP::HEARTBEAT);

    auto config_receiver = Configuration();
    config_receiver.set("_eor_timeout", 1);
    config_receiver.setArray<std::string>("_data_transmitters", {transmitter.getCanonicalName()});

    auto config_transmitter = Configuration();
    config_transmitter.set("_bor_timeout", 1);
    config_transmitter.set("_eor_timeout", 1);

    receiver.reactFSM(FSM::Transition::initialize, std::move(config_receiver));
    transmitter.reactFSM(FSM::Transition::initialize, std::move(config_transmitter));
    receiver.reactFSM(FSM::Transition::launch);
    transmitter.reactFSM(FSM::Transition::launch);
    receiver.reactFSM(FSM::Transition::start, "test");
    transmitter.reactFSM(FSM::Transition::start, "test");

    // Wait a bit for BOR to be handled by receiver
    receiver.awaitBOR();
    REQUIRE(receiver.getBOR(transmitter.getCanonicalName()).get<int>("_bor_timeout") == 1);

    // Allow receiver to progress through transitional state autonomously:
    receiver.skipTransitional(true);

    // Abort the transmitter - "failure" does not have a transitional state, so do not progress FSM:
    transmitter.reactFSM(FSM::Transition::failure, {}, false);

    // Wait until EOR is handled
    receiver.awaitEOR();
    const auto& eor = receiver.getEOR(transmitter.getCanonicalName());
    REQUIRE(eor.at("run_id").get<std::string>() == "test");
    REQUIRE(eor.at("condition").get<std::string>() == "ABORTED");
    REQUIRE(eor.at("condition_code").get<CDTP::RunCondition>() == CDTP::RunCondition::ABORTED);

    // Wait until receiver has handled interrupting
    while(receiver.getState() == FSM::State::interrupting) {
        std::this_thread::yield();
    }

    // Ensure receiver is in safe mode
    REQUIRE(receiver.getState() == FSM::State::SAFE);
    REQUIRE(transmitter.getState() == FSM::State::ERROR);

    receiver.exit();
    transmitter.exit();
    ManagerLocator::getCHIRPManager()->forgetDiscoveredServices();
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
