#include <chrono>
#include <iostream>
#include <future>
#include <thread>
#include <utility>

#include "asio.hpp"

#include "CHIRP/BroadcastSend.hpp"
#include "CHIRP/Manager.hpp"
#include "CHIRP/Message.hpp"

using namespace cnstln::CHIRP;
using namespace std::literals::chrono_literals;

int test_manager_sort_registered_service() {
    int fails = 0;
    // test self not smaller than self
    fails += RegisteredService({DATA, 0}) < RegisteredService({DATA, 0}) ? 1 : 0;
    // test service identifier takes priority over port
    fails += RegisteredService({CONTROL, 1}) < RegisteredService({DATA, 0}) ? 0 : 1;
    fails += RegisteredService({DATA, 0}) < RegisteredService({CONTROL, 1}) ? 1 : 0;
    // test sort after port if service identifier the same
    fails += RegisteredService({DATA, 0}) < RegisteredService({DATA, 1}) ? 0 : 1;
    return fails == 0 ? 0 : 1;
}

int test_manager_sort_discovered_service() {
    auto nh_s = MD5Hash("a");
    auto nh_l = MD5Hash("b");
    auto ip_1 = asio::ip::make_address("1.2.3.4");
    auto ip_2 = asio::ip::make_address("4.3.2.1");
    int fails = 0;
    // test self not smaller than self
    fails += DiscoveredService({ip_1, nh_s, DATA, 0}) < DiscoveredService({ip_1, nh_s, DATA, 0}) ? 1 : 0;
    // test ip does not change sorting
    fails += DiscoveredService({ip_1, nh_s, DATA, 0}) < DiscoveredService({ip_2, nh_s, DATA, 0}) ? 1 : 0;
    fails += DiscoveredService({ip_2, nh_s, DATA, 0}) < DiscoveredService({ip_1, nh_s, DATA, 0}) ? 1 : 0;
    // test name takes priority
    fails += DiscoveredService({ip_1, nh_s, DATA, 1}) < DiscoveredService({ip_1, nh_l, CONTROL, 0}) ? 0 : 1;
    fails += DiscoveredService({ip_1, nh_l, CONTROL, 0}) < DiscoveredService({ip_1, nh_s, DATA, 1}) ? 1 : 0;
    // test service identifier takes priority if same name
    fails += DiscoveredService({ip_1, nh_s, CONTROL, 1}) < DiscoveredService({ip_1, nh_s, DATA, 0}) ? 0 : 1;
    fails += DiscoveredService({ip_1, nh_s, DATA, 0}) < DiscoveredService({ip_1, nh_s, CONTROL, 1}) ? 1 : 0;
    // test port takes priority if same name and service identifier
    fails += DiscoveredService({ip_1, nh_s, DATA, 0}) < DiscoveredService({ip_1, nh_s, DATA, 1}) ? 0 : 1;
    return fails == 0 ? 0 : 1;
}

int test_manager_sort_discover_callback_entry() {
    auto* cb1 = reinterpret_cast<DiscoverCallback*>(1);
    auto* cb2 = reinterpret_cast<DiscoverCallback*>(2);
    auto* ud1 = reinterpret_cast<void*>(1);
    auto* ud2 = reinterpret_cast<void*>(2);
    int fails = 0;
    // test self not smaller than self
    fails += DiscoverCallbackEntry({cb1, DATA, ud1}) < DiscoverCallbackEntry({cb1, DATA, ud1}) ? 1 : 0;
    // test callback address takes priority
    fails += DiscoverCallbackEntry({cb1, DATA, ud2}) < DiscoverCallbackEntry({cb2, CONTROL, ud1}) ? 0 : 1;
    fails += DiscoverCallbackEntry({cb2, CONTROL, ud1}) < DiscoverCallbackEntry({cb1, DATA, ud2}) ? 1 : 0;
    // test service identifier takes priority if same callback address
    fails += DiscoverCallbackEntry({cb1, CONTROL, ud2}) < DiscoverCallbackEntry({cb1, DATA, ud1}) ? 0 : 1;
    fails += DiscoverCallbackEntry({cb1, DATA, ud1}) < DiscoverCallbackEntry({cb1, CONTROL, ud2}) ? 1 : 0;
    // test user data takes priority if same callback address and service identifier
    fails += DiscoverCallbackEntry({cb1, DATA, ud1}) < DiscoverCallbackEntry({cb1, DATA, ud2}) ? 0 : 1;
    return fails == 0 ? 0 : 1;
}

int test_manager_register_service_logic() {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};

    int fails = 0;
    // test that first register works
    auto registered = manager.RegisterService({CONTROL, 23999});
    fails += registered ? 0 : 1;
    // test that second register does not work
    auto regsitered_twice = manager.RegisterService({CONTROL, 23999});
    fails += regsitered_twice ? 1 : 0;
    // test that unregistering works
    auto unregistered = manager.UnregisterService({CONTROL, 23999});
    fails +=  unregistered ? 0 : 1;
    // test that unregistering for not registered service does not work
    auto unregistered_nonexist = manager.UnregisterService({CONTROL, 23999});
    fails += unregistered_nonexist ? 1 : 0;
    // test unregister all services
    manager.RegisterService({CONTROL, 23999});
    manager.RegisterService({CONTROL, 24000});
    fails += manager.GetRegisteredServices().size() == 2 ? 0 : 1;
    manager.UnregisterServices();
    fails += manager.GetRegisteredServices().size() == 0 ? 0 : 1;

    return fails == 0 ? 0 : 1;
}

int test_manager_register_callback_logic() {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};

    auto callback = [](DiscoveredService, bool, void*) {};

    int fails = 0;
    // test that first register works
    auto registered = manager.RegisterDiscoverCallback(callback, CONTROL, nullptr);
    fails += registered ? 0 : 1;
    // test that second register does not work
    auto regsitered_twice = manager.RegisterDiscoverCallback(callback, CONTROL, nullptr);
    fails += regsitered_twice ? 1 : 0;
    // test that unregistering works
    auto unregistered = manager.UnregisterDiscoverCallback(callback, CONTROL, nullptr);
    fails +=  unregistered ? 0 : 1;
    // test that unregistering for not registered service does not work
    auto unregistered_nonexist = manager.UnregisterDiscoverCallback(callback, CONTROL, nullptr);
    fails += unregistered_nonexist ? 1 : 0;
    // coverage test for unregister all services
    manager.RegisterDiscoverCallback(callback, CONTROL, nullptr);
    manager.RegisterDiscoverCallback(callback, HEARTBEAT, nullptr);
    manager.UnregisterDiscoverCallbacks();

    return fails == 0 ? 0 : 1;
}

int test_manager_async_timeout() {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    manager.Start();
    // This is purely a coverage test to ensure that the async receive works
    std::this_thread::sleep_for(105ms);
    return 0;
}

int test_manager_ignore_other_group() {
    BroadcastSend sender {"0.0.0.0"};
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    manager.Start();

    const auto asm_msg = Message(OFFER, "group2", "sat2", CONTROL, 23999).Assemble();
    sender.SendBroadcast(asm_msg.data(), asm_msg.size());

    return manager.GetDiscoveredServices().size() == 0 ? 0 : 1;
}

int test_manager_ignore_self() {
    BroadcastSend sender {"0.0.0.0"};
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    manager.Start();

    const auto asm_msg = Message(OFFER, "group1", "sat1", CONTROL, 23999).Assemble();
    sender.SendBroadcast(asm_msg.data(), asm_msg.size());

    return manager.GetDiscoveredServices().size() == 0 ? 0 : 1;
}

int test_manager_register_send_offer() {
    Manager manager {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    BroadcastRecv receiver {"0.0.0.0"};
    // Note: it seems we have to construct receiver after manager, else we do not receive messages (why?)

    // Start listening for OFFER message
    auto raw_msg_fut = std::async(&BroadcastRecv::RecvBroadcast, &receiver);
    // Register service, should send OFFER
    manager.RegisterService({CONTROL, 23999});
    // Receive message
    const auto raw_msg = raw_msg_fut.get();
    auto msg_from_manager = Message(AssembledMessage(raw_msg.content));
    // Check message
    int fails = 0;
    fails += msg_from_manager.GetType() == OFFER ? 0 : 1;
    fails += msg_from_manager.GetServiceIdentifier() == CONTROL ? 0 : 1;
    fails += msg_from_manager.GetPort() == 23999 ? 0 : 1;
    return fails == 0 ? 0 : 1;
}

int test_manager_discovery() {
    Manager manager1 {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    Manager manager2 {"0.0.0.0", "0.0.0.0", "group1", "sat2"};
    manager2.Start();

    int fails = 0;
    // Register service, should send OFFER
    manager1.RegisterService({DATA, 24000});
    // Wait a bit ensure we received the message
    std::this_thread::sleep_for(5ms);
    // Test that we discovered the service
    const auto services_1 = manager2.GetDiscoveredServices();
    if (services_1.size() == 1) {
        // Test that message is correct
        fails += services_1[0].name_hash == manager1.GetNameHash() ? 0 : 1;
        fails += services_1[0].ip == asio::ip::make_address("127.0.0.1") ? 0 : 1;
        fails += services_1[0].identifier == DATA ? 0 : 1;
        fails += services_1[0].port == 24000 ? 0 : 1;
    }
    else {
        fails += 1;
    }

    // Register other services
    manager1.RegisterService({MONITORING, 65000});
    manager1.RegisterService({HEARTBEAT, 65001});
    std::this_thread::sleep_for(5ms);
    // Test that we discovered the services
    fails += manager2.GetDiscoveredServices().size() == 3 ? 0 : 1;
    // Unregister a service
    manager1.UnregisterService({MONITORING, 65000});
    std::this_thread::sleep_for(5ms);
    // Test that we discovered LEAVING message
    fails += manager2.GetDiscoveredServices().size() == 2 ? 0 : 1;
    // Now test that we can filter a service category
    fails += manager2.GetDiscoveredServices(HEARTBEAT).size() == 1 ? 0 : 1;
    // Test that we can forget services
    manager2.ForgetDiscoveredServices();
    fails += manager2.GetDiscoveredServices().size() == 0 ? 0 : 1;

    // Register new services
    manager1.UnregisterServices();
    manager1.RegisterService({CONTROL, 40001});
    manager1.RegisterService({DATA, 40002});
    std::this_thread::sleep_for(5ms);
    // Test that we discovered services
    fails += manager2.GetDiscoveredServices().size() == 2 ? 0 : 1;
    // Unregister all services
    manager1.UnregisterServices();
    std::this_thread::sleep_for(5ms);
    // Test that we discovered LEAVING messages
    fails += manager2.GetDiscoveredServices().size() == 0 ? 0 : 1;

    return fails == 0 ? 0 : 1;
}

int test_manager_callbacks() {
    Manager manager1 {"0.0.0.0", "0.0.0.0", "group1", "sat1"};
    Manager manager2 {"0.0.0.0", "0.0.0.0", "group1", "sat2"};
    manager2.Start();

    // Create a callback, use pointer to access test variable
    std::pair<bool, DiscoveredService> cb_leaving_service {true, {}};
    auto callback = [](DiscoveredService service, bool leaving, void* cb_info) {
        auto cb_leaving_service_l = static_cast<std::pair<bool, DiscoveredService>*>(cb_info);
        cb_leaving_service_l->first = leaving;
        cb_leaving_service_l->second = std::move(service);
    };

    int fails = 0;
    // Register callback for CONTROL
    manager2.RegisterDiscoverCallback(callback, CONTROL, &cb_leaving_service);
    // Register CONTROL service
    manager1.RegisterService({CONTROL, 50100});
    // Wait a bit ensure the callback is executed
    std::this_thread::sleep_for(5ms);
    // Test that we correct OFFER callback
    fails += cb_leaving_service.first ? 1 : 0;
    fails += cb_leaving_service.second.identifier == CONTROL ? 0 : 1;
    fails += cb_leaving_service.second.port == 50100 ? 0 : 1;

    // Unregister service
    manager1.UnregisterService({CONTROL, 50100});
    std::this_thread::sleep_for(5ms);
    // Test that we got LEAVING callback
    fails += cb_leaving_service.first ? 0 : 1;

    // Unregister callback
    manager2.UnregisterDiscoverCallback(callback, CONTROL, &cb_leaving_service);
    // Register CONTROL service
    manager1.RegisterService({CONTROL, 50100});
    std::this_thread::sleep_for(5ms);
    // Test that we did not get an OFFER callback but still are at LEAVING from before
    fails += cb_leaving_service.first ? 0 : 1;

    // Register callback for HEARTBEAT and MONITORING
    manager2.RegisterDiscoverCallback(callback, HEARTBEAT, &cb_leaving_service);
    manager2.RegisterDiscoverCallback(callback, MONITORING, &cb_leaving_service);
    // Register HEARTBEAT service
    manager1.RegisterService({HEARTBEAT, 50200});
    std::this_thread::sleep_for(5ms);
    // Test that we got HEARTBEAT callback
    fails += cb_leaving_service.second.identifier == HEARTBEAT ? 0 : 1;
    // Register MONITORING service
    manager1.RegisterService({MONITORING, 50300});
    std::this_thread::sleep_for(5ms);
    // Test that we got MONITORING callback
    fails += cb_leaving_service.second.identifier == MONITORING ? 0 : 1;

    // Unregister all callback
    manager2.UnregisterDiscoverCallbacks();
    // Unregister all services
    manager1.UnregisterServices();
    std::this_thread::sleep_for(5ms);
    // Test that we did not get a LEAVING callback but still are at OFFER from before
    fails += cb_leaving_service.first ? 1 : 0;

    return fails == 0 ? 0 : 1;
}

// TODO: test request
// TODO: test decode error

int main() {
    int ret = 0;
    int ret_test = 0;

    // test_manager_sort_registered_service
    std::cout << "test_manager_sort_registered_service...      " << std::flush;
    ret_test = test_manager_sort_registered_service();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_manager_sort_discovered_service
    std::cout << "test_manager_sort_discovered_service...      " << std::flush;
    ret_test = test_manager_sort_discovered_service();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_manager_sort_discover_callback_entry
    std::cout << "test_manager_sort_discover_callback_entry... " << std::flush;
    ret_test = test_manager_sort_discover_callback_entry();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_manager_register_service_logic
    std::cout << "test_manager_register_service_logic...       " << std::flush;
    ret_test = test_manager_register_service_logic();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_manager_register_callback_logic
    std::cout << "test_manager_register_callback_logic...      " << std::flush;
    ret_test = test_manager_register_callback_logic();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_manager_async_timeout
    std::cout << "test_manager_async_timeout...                " << std::flush;
    ret_test = test_manager_async_timeout();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_manager_ignore_other_group
    std::cout << "test_manager_ignore_other_group...           " << std::flush;
    ret_test = test_manager_ignore_other_group();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_manager_ignore_self
    std::cout << "test_manager_ignore_self...                  " << std::flush;
    ret_test = test_manager_ignore_self();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_manager_register_send_offer
    std::cout << "test_manager_register_send_offer...          " << std::flush;
    ret_test = test_manager_register_send_offer();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_manager_discovery
    std::cout << "test_manager_discovery...                    " << std::flush;
    ret_test = test_manager_discovery();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    // test_manager_callbacks
    std::cout << "test_manager_callbacks...                    " << std::flush;
    ret_test = test_manager_callbacks();
    std::cout << (ret_test == 0 ? " passed" : " failed") << std::endl;
    ret += ret_test;

    if (ret == 0) {
        std::cout << "\nAll tests passed" << std::endl;
    }
    else {
        std::cout << "\n" << ret << " tests failed" << std::endl;
    }
    return ret;
}
