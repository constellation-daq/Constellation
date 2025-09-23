/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <cstddef>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "constellation/build.hpp"
#include "constellation/core/config/Configuration.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/message/CDTP2Message.hpp"
#include "constellation/core/protocol/CDTP_definitions.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/utils/enum.hpp" // IWYU pragma: keep
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/satellite/FSM.hpp"
#include "constellation/satellite/ReceiverSatellite.hpp"
#include "constellation/satellite/TransmitterSatellite.hpp"

#include "chirp_mock.hpp"
#include "dummy_satellite.hpp"

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
    void receive_bor(std::string_view sender, const Dictionary& user_tags, const Configuration& config) override {
        const auto sender_str = std::string(sender);
        const std::lock_guard map_lock {map_mutex_};
        bor_map_.erase(sender_str);
        bor_map_.emplace(sender, Configuration(config.getDictionary()));
        bor_tag_map_.erase(sender_str);
        bor_tag_map_.emplace(sender, user_tags);
        bor_received_ = true;
    }
    void receive_data(std::string_view sender, const CDTP2Message::DataRecord& data_record) override {
        const auto sender_str = std::string(sender);
        const std::lock_guard map_lock {map_mutex_};
        last_data_map_.erase(sender_str);
        last_data_map_.emplace(sender, copy_record(data_record));
        data_received_ = true;
    }
    void receive_eor(std::string_view sender, const Dictionary& user_tags, const Dictionary& run_metadata) override {
        const auto sender_str = std::string(sender);
        const std::lock_guard map_lock {map_mutex_};
        eor_map_.erase(sender_str);
        eor_map_.emplace(sender, run_metadata);
        eor_tag_map_.erase(sender_str);
        eor_tag_map_.emplace(sender, user_tags);
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
    const CDTP2Message::DataRecord& getLastData(const std::string& sender) {
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
    static CDTP2Message::DataRecord copy_record(const CDTP2Message::DataRecord& data_record) {
        // Data records cannot be copied for good reason, but we need to for testing purposes
        CDTP2Message::DataRecord record_copy {
            data_record.getSequenceNumber(), data_record.getTags(), data_record.countBlocks()};
        for(const auto& block : data_record.getBlocks()) {
            std::vector<std::byte> block_copy {block.span().begin(), block.span().end()};
            record_copy.addBlock(std::move(block_copy));
        }
        return record_copy;
    }

private:
    std::mutex map_mutex_;
    std::atomic_bool bor_received_ {false};
    std::atomic_bool data_received_ {false};
    std::atomic_bool eor_received_ {false};
    std::map<std::string, Configuration> bor_map_;
    std::map<std::string, Dictionary> bor_tag_map_;
    std::map<std::string, CDTP2Message::DataRecord> last_data_map_;
    std::map<std::string, Dictionary> eor_map_;
    std::map<std::string, Dictionary> eor_tag_map_;
};

class Transmitter : public DummySatellite<TransmitterSatellite> {
public:
    Transmitter(std::string_view name = "t1") : DummySatellite<TransmitterSatellite>(name) {}

    template <typename T> void sendData(T data) {
        auto data_record = newDataRecord();
        data_record.addBlock(std::move(data));
        data_record.addTag("test", 1);
        sendDataRecord(std::move(data_record));
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

TEST_CASE("Transmitter / EOR timeout", "[satellite]") {
    // Create CHIRP manager for data service discovery
    create_chirp_manager();

    auto transmitter = Transmitter();
    transmitter.mockChirpService(CHIRP::DATA);

    auto receiver = Receiver();
    auto config_receiver = Configuration();
    config_receiver.set("_eor_timeout", 1);
    config_receiver.setArray<std::string>("_data_transmitters", {transmitter.getCanonicalName()});

    auto config_transmitter = Configuration();
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

    // Stop the transmitter to send EOR
    transmitter.reactFSM(FSM::Transition::stop);

    // Check that transmitter went to ERROR since EOR was not received
    REQUIRE(transmitter.getState() == FSM::State::ERROR);

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
    config2_transmitter.set("_data_timeout", 1);
    config2_transmitter.set("_eor_timeout", 1);
    config2_transmitter.set("_data_timeout", 1);
    config2_transmitter.set("_payload_threshold", 0);
    config2_transmitter.set("_queue_size", 2);
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

    // Send data
    transmitter.sendData(std::vector<int>({1, 2, 3, 4}));
    REQUIRE(transmitter.canSendRecord());
    // Wait a bit for data to be handled by receiver
    receiver.awaitData();
    const auto& data_record = receiver.getLastData(transmitter.getCanonicalName());
    REQUIRE(data_record.countBlocks() == 1);
    REQUIRE(data_record.getTags().at("test") == 1);

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
    config_transmitter.set("_payload_threshold", 1024);
    config_transmitter.set("_queue_size", 2);

    receiver.reactFSM(FSM::Transition::initialize, std::move(config_receiver));
    transmitter.reactFSM(FSM::Transition::initialize, std::move(config_transmitter));
    receiver.reactFSM(FSM::Transition::launch);
    transmitter.reactFSM(FSM::Transition::launch);
    receiver.reactFSM(FSM::Transition::start, "test");
    transmitter.reactFSM(FSM::Transition::start, "test");

    // Wait a bit for BOR to be handled by receiver
    receiver.awaitBOR();
    REQUIRE(receiver.getBOR(transmitter.getCanonicalName()).get<int>("_bor_timeout") == 1);

    // Send a data
    transmitter.sendData(std::vector<int>({1, 2, 3, 4}));
    // Wait a bit for data to be handled by receiver
    receiver.awaitData();
    const auto& data_record = receiver.getLastData(transmitter.getCanonicalName());
    REQUIRE(data_record.countBlocks() == 1);
    REQUIRE(data_record.getTags().at("test") == 1);

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

    // Ensure all satellites are in safe mode
    REQUIRE(transmitter.getState() == FSM::State::SAFE);
    auto receiver_state = receiver.getState();
    while(receiver_state == FSM::State::RUN || receiver_state == FSM::State::interrupting) {
        std::this_thread::yield();
        receiver_state = receiver.getState();
    }
    REQUIRE(receiver_state == FSM::State::SAFE);

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
    REQUIRE(eor.at("condition").get<std::string>() == "TAINTED|ABORTED");
    REQUIRE(eor.at("condition_code").get<CDTP::RunCondition>() ==
            (CDTP::RunCondition::TAINTED | CDTP::RunCondition::ABORTED));

    // Wait until receiver has handled interrupting
    const auto run_interrupting = std::set<FSM::State>({FSM::State::RUN, FSM::State::interrupting});
    while(run_interrupting.contains(receiver.getState())) {
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
