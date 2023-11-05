#include "Manager.hpp"

#include <functional>
#include <utility>

#include "CHIRP/exceptions.hpp"

using namespace cnstln::CHIRP;

bool RegisteredService::operator<(const RegisteredService& other) const {
    // Sort first by service id, then by port
    auto ord_id = std::to_underlying(identifier) <=> std::to_underlying(other.identifier);
    if (std::is_lt(ord_id)) {
        return true;
    }
    else if (std::is_eq(ord_id)) {
        return port < other.port;
    }
    return false;
}

bool DiscoveredService::operator<(const DiscoveredService& other) const {
    auto ord_md5 = name_hash <=> other.name_hash;
    if (std::is_lt(ord_md5)) {
        return true;
    }
    if (std::is_gt(ord_md5)) {
        return false;
    }
    // Same as RegisteredService::operator<
    auto ord_id = std::to_underlying(identifier) <=> std::to_underlying(other.identifier);
    if (std::is_lt(ord_id)) {
        return true;
    }
    else if (std::is_eq(ord_id)) {
        return port < other.port;
    }
    return false;
}

Manager::Manager(asio::io_context& io_context, std::string_view group, std::string_view name)
  : receiver_(io_context), sender_(io_context),
    group_hash_(MD5Hash(group)), name_hash_(MD5Hash(name)) {}

Manager::~Manager() {
    // First stop Run function
    run_thread_.request_stop();
    if (run_thread_.joinable()) {
        run_thread_.join();
    }
    // Now unregister all services
    UnregisterServices();
}

void Manager::Start() {
    // jthread immediatly starts on construction
    run_thread_ = std::jthread(std::bind_front(&Manager::Run, this));
}

void Manager::RegisterService(RegisteredService service) {
    if (registered_services_.contains(service)) {
        // TODO throw
    }
    registered_services_.insert(service);
    SendOffer(service);
}

void Manager::UnregisterService(RegisteredService service) {
    if (!registered_services_.contains(service)) {
        // TODO throw
    }
    registered_services_.erase(service);
    // Send unsubscribe message
    service.port = 0;
    SendOffer(service);
}

void Manager::UnregisterServices() {
    for (auto service : registered_services_) {
        // Send unsubscribe message
        service.port = 0;
        SendOffer(service);
    }
    registered_services_.clear();
}

void Manager::SendOffer(RegisteredService service) {
    const auto asm_msg = Message(OFFER, group_hash_, name_hash_, service.identifier, service.port).Assemble();
    sender_.SendBroadcast(asm_msg.data(), asm_msg.size());
}

void Manager::Run(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        try {
            auto raw_msg = receiver_.RecvBroadcast();  // TODO: timeout

            auto chirp_msg = Message(AssembledMessage(std::move(raw_msg.content)));

            if (chirp_msg.GetGroupHash() != group_hash_) {
                // Broadcast from different group, ignore
                continue;
            }
            if (chirp_msg.GetNameHash() != name_hash_) {
                // Broadcast from self, ignore
                continue;
            }

            DiscoveredService discovered_service {chirp_msg.GetNameHash(), chirp_msg.GetServiceIdentifier(), chirp_msg.GetPort()};

            switch (chirp_msg.GetType()) {
            case REQUEST: {
                auto service_id = discovered_service.identifier;
                for (const auto& service : registered_services_) {
                    if (service.identifier == service_id) {
                        SendOffer(service);
                    }
                }
                break;
            }
            case OFFER: {
                if (!discovered_services_.contains(discovered_service)) {
                    discovered_services_.insert(discovered_service);
                    // TODO user callback
                }
                break;
            }
            case LEAVING: {
                if (discovered_services_.contains(discovered_service)) {
                    discovered_services_.erase(discovered_service);
                    // TODO user callback
                }
                break;
            }
            default:
                std::unreachable();
            }
        }
        catch (const DecodeError& error) {
            continue;
        }
    }
}
