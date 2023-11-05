#pragma once

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
    bool operator<(const RegisteredService& other) const;
};

struct DiscoveredService {
    MD5Hash name_hash;
    ServiceIdentifier identifier;
    Port port;
    bool operator<(const DiscoveredService& other) const;
};

class Manager {
public:
    Manager(asio::io_context& io_context, std::string_view group, std::string_view name);
    virtual ~Manager();

    void Start();

    void RegisterService(RegisteredService service);
    void UnregisterService(RegisteredService service);
    void UnregisterServices();

    std::set<DiscoveredService> GetDiscoveredServices();

private:
    void SendOffer(RegisteredService service);
    void Run(std::stop_token stop_token);

private:
    std::jthread run_thread_;
    std::set<RegisteredService> registered_services_;  // not thread safe!
    std::set<DiscoveredService> discovered_services_;  // not thread safe!
    BroadcastRecv receiver_;
    BroadcastSend sender_;
    MD5Hash group_hash_;
    MD5Hash name_hash_;
};

}
}
