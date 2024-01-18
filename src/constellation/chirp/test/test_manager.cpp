/**
 * @file
 * @brief Tests for CHIRP manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <any>
#include <chrono>
#include <future>
#include <thread>
#include <utility>

#include <asio.hpp>
#include <catch2/catch_test_macros.hpp>

#include "constellation/chirp/BroadcastRecv.hpp"
#include "constellation/chirp/BroadcastSend.hpp"
#include "constellation/chirp/Manager.hpp"
#include "constellation/chirp/Message.hpp"
#include "constellation/chirp/protocol_info.hpp"

using namespace constellation::chirp;
using namespace std::literals::chrono_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Sorting of registered services", "[chirp]") {
    // test self not smaller than self
    REQUIRE_FALSE(RegisteredService({DATA, 0}) < RegisteredService({DATA, 0}));
    // test service identifier takes priority over port
    REQUIRE(RegisteredService({CONTROL, 1}) < RegisteredService({DATA, 0}));
    REQUIRE_FALSE(RegisteredService({DATA, 0}) < RegisteredService({CONTROL, 1}));
    // test sort after port if service identifier the same
    REQUIRE(RegisteredService({DATA, 0}) < RegisteredService({DATA, 1}));
}

TEST_CASE("Sorting of discovered services", "[chirp]") {
    auto id1 = MD5Hash("a");
    auto id2 = MD5Hash("b");
    auto ip1 = asio::ip::make_address("1.2.3.4");
    auto ip2 = asio::ip::make_address("4.3.2.1");

    // test self not smaller than self
    REQUIRE_FALSE(DiscoveredService({ip1, id1, DATA, 0}) < DiscoveredService({ip1, id1, DATA, 0}));
    // test ip does not change sorting
    REQUIRE_FALSE(DiscoveredService({ip1, id1, DATA, 0}) < DiscoveredService({ip2, id1, DATA, 0}));
    REQUIRE_FALSE(DiscoveredService({ip2, id1, DATA, 0}) < DiscoveredService({ip1, id1, DATA, 0}));
    // test host takes priority
    REQUIRE(DiscoveredService({ip1, id1, DATA, 1}) < DiscoveredService({ip1, id2, CONTROL, 0}));
    REQUIRE_FALSE(DiscoveredService({ip1, id2, CONTROL, 0}) < DiscoveredService({ip1, id1, DATA, 1}));
    // test service identifier takes priority if same host
    REQUIRE(DiscoveredService({ip1, id1, CONTROL, 1}) < DiscoveredService({ip1, id1, DATA, 0}));
    REQUIRE_FALSE(DiscoveredService({ip1, id1, DATA, 0}) < DiscoveredService({ip1, id1, CONTROL, 1}));
    // test port takes priority if same host and service identifier
    REQUIRE(DiscoveredService({ip1, id1, DATA, 0}) < DiscoveredService({ip1, id1, DATA, 1}));
}

TEST_CASE("Sorting of discover callbacks", "[chirp]") {
    auto* cb1 = reinterpret_cast<DiscoverCallback*>(1); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* cb2 = reinterpret_cast<DiscoverCallback*>(2); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    auto ud1 = std::make_any<int>(1);
    auto ud2 = std::make_any<int>(2);

    // test self not smaller than self
    REQUIRE_FALSE(DiscoverCallbackEntry({cb1, DATA, ud1}) < DiscoverCallbackEntry({cb1, DATA, ud1}));
    // test user data does not change sorting
    REQUIRE_FALSE(DiscoverCallbackEntry({cb1, DATA, ud1}) < DiscoverCallbackEntry({cb1, DATA, ud2}));
    REQUIRE_FALSE(DiscoverCallbackEntry({cb1, DATA, ud2}) < DiscoverCallbackEntry({cb1, DATA, ud1}));
    // test callback address takes priority
    REQUIRE(DiscoverCallbackEntry({cb1, DATA, ud1}) < DiscoverCallbackEntry({cb2, CONTROL, ud1}));
    REQUIRE_FALSE(DiscoverCallbackEntry({cb2, CONTROL, ud1}) < DiscoverCallbackEntry({cb1, DATA, ud1}));
    // test service identifier takes priority if same callback address
    REQUIRE(DiscoverCallbackEntry({cb1, CONTROL, ud1}) < DiscoverCallbackEntry({cb1, DATA, ud1}));
}

TEST_CASE("Register services in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};

    // test that first register works
    REQUIRE(manager.RegisterService(CONTROL, 23999));
    // test that second register does not work
    REQUIRE_FALSE(manager.RegisterService(CONTROL, 23999));
    // test that unregistering works
    REQUIRE(manager.UnregisterService(CONTROL, 23999));
    // test that unregistering for not registered service does not work
    REQUIRE_FALSE(manager.UnregisterService(CONTROL, 23999));
    // test unregister all services
    manager.RegisterService(CONTROL, 23999);
    manager.RegisterService(CONTROL, 24000);
    REQUIRE(manager.GetRegisteredServices().size() == 2);
    manager.UnregisterServices();
    REQUIRE(manager.GetRegisteredServices().empty());
}

TEST_CASE("Register callbacks in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};

    // DiscoverCallback signature NOLINTNEXTLINE(performance-unnecessary-value-param)
    auto callback = [](DiscoveredService, bool, std::any) {};

    // test that first register works
    REQUIRE(manager.RegisterDiscoverCallback(callback, CONTROL, nullptr));
    // test that second register does not work
    REQUIRE_FALSE(manager.RegisterDiscoverCallback(callback, CONTROL, nullptr));
    // test that unregistering works
    REQUIRE(manager.UnregisterDiscoverCallback(callback, CONTROL));
    // test that unregistering for not registered service does not work
    REQUIRE_FALSE(manager.UnregisterDiscoverCallback(callback, CONTROL));

    // coverage test for unregister all services
    manager.RegisterDiscoverCallback(callback, CONTROL, nullptr);
    manager.RegisterDiscoverCallback(callback, HEARTBEAT, nullptr);
    manager.UnregisterDiscoverCallbacks();
}

TEST_CASE("Get async timeout in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    manager.Start();
    // This is purely a coverage test to ensure that the async receive works
    std::this_thread::sleep_for(105ms);
}

TEST_CASE("Ignore CHIRP message from other group in CHIRP manager", "[chirp][chirp::manager]") {
    BroadcastSend sender {"0.0.0.0", CHIRP_PORT};
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    manager.Start();

    const auto asm_msg = Message(OFFER, "group2", "sat2", CONTROL, 23999).Assemble();
    sender.SendBroadcast(asm_msg.data(), asm_msg.size());

    REQUIRE(manager.GetDiscoveredServices().empty());
}

TEST_CASE("Ignore CHIRP message from self in CHIRP manager", "[chirp][chirp::manager]") {
    BroadcastSend sender {"0.0.0.0", CHIRP_PORT};
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    manager.Start();

    const auto asm_msg = Message(OFFER, "group1", "sat1", CONTROL, 23999).Assemble();
    sender.SendBroadcast(asm_msg.data(), asm_msg.size());

    REQUIRE(manager.GetDiscoveredServices().empty());
}

TEST_CASE("Discover services in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager1 {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    Manager manager2 {"0.0.0.0", "0.0.0.0", "group1", "sat2"};
    manager2.Start();

    // Register service, should send OFFER
    manager1.RegisterService(DATA, 24000);
    // Wait a bit ensure we received the message
    std::this_thread::sleep_for(5ms);
    // Test that we discovered the service
    const auto services_1 = manager2.GetDiscoveredServices();
    REQUIRE(services_1.size() == 1);

    // Test that message is correct
    REQUIRE(services_1[0].host_id == manager1.GetHostID());
    REQUIRE(services_1[0].address == asio::ip::make_address("127.0.0.1"));
    REQUIRE(services_1[0].identifier == DATA);
    REQUIRE(services_1[0].port == 24000);

    // Register other services
    manager1.RegisterService(MONITORING, 65000);
    manager1.RegisterService(HEARTBEAT, 65001);
    std::this_thread::sleep_for(5ms);

    // Test that we discovered the services
    REQUIRE(manager2.GetDiscoveredServices().size() == 3);
    // Unregister a service
    manager1.UnregisterService(MONITORING, 65000);
    std::this_thread::sleep_for(5ms);
    // Test that we discovered DEPART message
    REQUIRE(manager2.GetDiscoveredServices().size() == 2);
    // Now test that we can filter a service category
    REQUIRE(manager2.GetDiscoveredServices(HEARTBEAT).size() == 1);
    // Test that we can forget services
    manager2.ForgetDiscoveredServices();
    REQUIRE(manager2.GetDiscoveredServices().empty());

    // Register new services
    manager1.UnregisterServices();
    manager1.RegisterService(CONTROL, 40001);
    manager1.RegisterService(DATA, 40002);
    std::this_thread::sleep_for(5ms);
    // Test that we discovered services
    REQUIRE(manager2.GetDiscoveredServices().size() == 2);
    // Unregister all services
    manager1.UnregisterServices();
    std::this_thread::sleep_for(5ms);
    // Test that we discovered DEPART messages
    REQUIRE(manager2.GetDiscoveredServices().empty());
}

TEST_CASE("Execute callbacks in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager1 {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    Manager manager2 {"0.0.0.0", "0.0.0.0", "group1", "sat2"};
    manager2.Start();

    // Create a callback, use pointer to access test variable
    std::pair<bool, DiscoveredService> cb_departb_service {true, {}};
    auto callback = [](DiscoveredService service, bool depart, std::any cb_info) {
        auto* cb_departb_service_l = std::any_cast<std::pair<bool, DiscoveredService>*>(cb_info);
        cb_departb_service_l->first = depart;
        cb_departb_service_l->second = std::move(service);
    };

    // Register callback for CONTROL
    manager2.RegisterDiscoverCallback(callback, CONTROL, &cb_departb_service);
    // Register CONTROL service
    manager1.RegisterService(CONTROL, 50100);
    // Wait a bit ensure the callback is executed
    std::this_thread::sleep_for(5ms);
    // Test that we correct OFFER callback
    REQUIRE_FALSE(cb_departb_service.first);
    REQUIRE(cb_departb_service.second.identifier == CONTROL);
    REQUIRE(cb_departb_service.second.port == 50100);

    // Unregister service
    manager1.UnregisterService(CONTROL, 50100);
    std::this_thread::sleep_for(5ms);
    // Test that we got DEPART callback
    REQUIRE(cb_departb_service.first);

    // Unregister callback
    manager2.UnregisterDiscoverCallback(callback, CONTROL);
    // Register CONTROL service
    manager1.RegisterService(CONTROL, 50100);
    std::this_thread::sleep_for(5ms);
    // Test that we did not get an OFFER callback but still are at DEPART from before
    REQUIRE(cb_departb_service.first);

    // Register callback for HEARTBEAT and MONITORING
    manager2.RegisterDiscoverCallback(callback, HEARTBEAT, &cb_departb_service);
    manager2.RegisterDiscoverCallback(callback, MONITORING, &cb_departb_service);
    // Register HEARTBEAT service
    manager1.RegisterService(HEARTBEAT, 50200);
    std::this_thread::sleep_for(5ms);
    // Test that we got HEARTBEAT callback
    REQUIRE(cb_departb_service.second.identifier == HEARTBEAT);
    // Register MONITORING service
    manager1.RegisterService(MONITORING, 50300);
    std::this_thread::sleep_for(5ms);
    // Test that we got MONITORING callback
    REQUIRE(cb_departb_service.second.identifier == MONITORING);

    // Unregister all callback
    manager2.UnregisterDiscoverCallbacks();
    // Unregister all services
    manager1.UnregisterServices();
    std::this_thread::sleep_for(5ms);
    // Test that we did not get a DEPART callback but still are at OFFER from before
    REQUIRE_FALSE(cb_departb_service.first);
}

TEST_CASE("Send CHIRP requests in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    BroadcastRecv receiver {"0.0.0.0", CHIRP_PORT};
    // Note: it seems we have to construct receiver after manager, else we do not receive messages
    // Why? we can only have one working recv binding to the same socket per process unfortunately :/

    // Start listening for request message
    auto raw_msg_fut = std::async(&BroadcastRecv::RecvBroadcast, &receiver);
    // Send request
    manager.SendRequest(CONTROL);
    // Receive message
    const auto raw_msg = raw_msg_fut.get();
    auto msg_from_manager = Message(raw_msg.content);
    // Check message
    REQUIRE(msg_from_manager.GetType() == REQUEST);
    REQUIRE(msg_from_manager.GetServiceIdentifier() == CONTROL);
    REQUIRE(msg_from_manager.GetPort() == 0);
}

TEST_CASE("Receive CHIRP requests in CHIRP manager", "[chirp][chirp::manager]") {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    BroadcastSend sender {"0.0.0.0", CHIRP_PORT};
    // Note: we cannot test if an offer is actually replied, see `test_manager_send_request`

    // Register service
    manager.Start();
    manager.RegisterService(CONTROL, 45454);
    // Send requests
    const auto asm_msg_a = Message(REQUEST, "group1", "sat2", CONTROL, 0).Assemble();
    const auto asm_msg_b = Message(REQUEST, "group1", "sat2", DATA, 0).Assemble();
    sender.SendBroadcast(asm_msg_a.data(), asm_msg_a.size());
    sender.SendBroadcast(asm_msg_b.data(), asm_msg_b.size());
    // Wait a bit ensure we received the message
    std::this_thread::sleep_for(5ms);

    // If everything worked, the corresponding lines should be marked as executed in coverage
}

TEST_CASE("Detect incorrect CHIRP message in CHIRP manager", "[chirp][chirp::manager]") {
    BroadcastSend sender {"0.0.0.0", CHIRP_PORT};
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    manager.Start();

    // Create invalid message
    auto asm_msg = Message(REQUEST, "group1", "sat2", CONTROL, 0).Assemble();
    asm_msg[0] = 'X';
    // Send message
    sender.SendBroadcast(asm_msg.data(), asm_msg.size());
    // Wait a bit ensure we received the message
    std::this_thread::sleep_for(5ms);

    // If everything worked, the corresponding lines should be marked as executed in coverage
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
