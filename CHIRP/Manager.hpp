#pragma once

#include <mutex>
#include <set>
#include <string_view>
#include <thread>

#include "asio.hpp"

#include "CHIRP/config.hpp"
#include "CHIRP/BroadcastRecv.hpp"
#include "CHIRP/BroadcastSend.hpp"
#include "CHIRP/Message.hpp"

namespace cnstln {
namespace CHIRP {

struct RegisteredService {
    ServiceIdentifier identifier;
    Port port;
    CHIRP_API bool operator<(const RegisteredService& other) const;
};

struct DiscoveredService {
    asio::ip::address ip;
    MD5Hash name_hash;
    ServiceIdentifier identifier;
    Port port;
    CHIRP_API bool operator<(const DiscoveredService& other) const;
};

using DiscoverCallback = void(DiscoveredService service, bool leaving, void* user_data);

struct DiscoverCallbackEntry {
    DiscoverCallback* callback;
    ServiceIdentifier service;
    void* user_data;
    CHIRP_API bool operator<(const DiscoverCallbackEntry& other) const;
};

class Manager {
public:
    CHIRP_API Manager(asio::ip::address brd_address, asio::ip::address any_address, std::string_view group, std::string_view name);
    CHIRP_API Manager(std::string_view brd_ip, std::string_view any_ip, std::string_view group, std::string_view name);
    CHIRP_API virtual ~Manager();

    CHIRP_API void Start();

    CHIRP_API bool RegisterService(RegisteredService service);
    CHIRP_API bool UnregisterService(RegisteredService service);
    CHIRP_API void UnregisterServices();
    CHIRP_API std::set<RegisteredService> GetRegisteredServices();

    CHIRP_API bool RegisterDiscoverCallback(DiscoverCallback* callback, ServiceIdentifier service, void* user_data);
    CHIRP_API bool UnregisterDiscoverCallback(DiscoverCallback* callback, ServiceIdentifier service, void* user_data);
    CHIRP_API void UnregisterDiscoverCallbacks();

    CHIRP_API void ForgetDiscoveredServices();
    CHIRP_API std::set<DiscoveredService> GetDiscoveredServices();

    CHIRP_API void SendRequest(ServiceIdentifier service);

private:
    void SendMessage(MessageType type, RegisteredService service);
    void Run(std::stop_token stop_token);

private:
    BroadcastRecv receiver_;
    BroadcastSend sender_;
    MD5Hash group_hash_;
    MD5Hash name_hash_;
    std::set<RegisteredService> registered_services_;
    std::set<DiscoveredService> discovered_services_;
    std::set<DiscoverCallbackEntry> discover_callbacks_;
    std::mutex registered_services_mutex_;
    std::mutex discovered_services_mutex_;
    std::mutex discover_callbacks_mutex_;
    std::jthread run_thread_;
};

}
}
