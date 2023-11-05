#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

#include "asio.hpp"

#include "CHIRP/BroadcastSend.hpp"
#include "CHIRP/Manager.hpp"
#include "CHIRP/Message.hpp"

using namespace cnstln::CHIRP;
using namespace std::literals::chrono_literals;
using namespace std::literals::string_view_literals;

// TODO: test sorting functions

int test_manager_ignore_other_group() {
    BroadcastSend sender {"0.0.0.0"sv};
    Manager manager {"0.0.0.0"sv, "0.0.0.0"sv, "group1"sv, "sat1"sv};
    manager.Start();

    const auto asm_msg = Message(OFFER, "group2", "sat2", CONTROL, 23999).Assemble();
    sender.SendBroadcast(asm_msg.data(), asm_msg.size());

    return manager.GetDiscoveredServices().size() == 0 ? 0 : 1;
}

int test_manager_ignore_self() {
    BroadcastSend sender {"0.0.0.0"sv};
    Manager manager {"0.0.0.0"sv, "0.0.0.0"sv, "group1"sv, "sat1"sv};
    manager.Start();

    const auto asm_msg = Message(OFFER, "group1", "sat1", CONTROL, 23999).Assemble();
    sender.SendBroadcast(asm_msg.data(), asm_msg.size());

    return manager.GetDiscoveredServices().size() == 0 ? 0 : 1;
}

int test_manager_async_timeout() {
    Manager manager {"0.0.0.0"sv, "0.0.0.0"sv, "group1"sv, "sat1"sv};
    manager.Start();
    // This is purely a coverage test to ensure that the async receive works
    std::this_thread::sleep_for(110ms);
    return 0;
}

int test_manager_register_service_logic() {
    Manager manager {"0.0.0.0"sv, "0.0.0.0"sv, "group1"sv, "sat1"sv};
    manager.Start();

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
    Manager manager {"0.0.0.0"sv, "0.0.0.0"sv, "group1"sv, "sat1"sv};
    manager.Start();

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

int main() {
    int ret = 0;
    int ret_test = 0;

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

    // test_manager_async_timeout
    std::cout << "test_manager_async_timeout...                " << std::flush;
    ret_test = test_manager_async_timeout();
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

    if (ret == 0) {
        std::cout << "\nAll tests passed" << std::endl;
    }
    else {
        std::cout << "\n" << ret << " tests failed" << std::endl;
    }
    return ret;
}
