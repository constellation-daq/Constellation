/**
 * @file
 * @brief Implementation of the CHIRP manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Manager.hpp"

#include <algorithm>
#include <any>
#include <chrono>
#include <compare>
#include <cstdint>
#include <functional>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <asio/ip/address_v4.hpp>

#include "constellation/core/chirp/MulticastSocket.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/networking/asio_helpers.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/std_future.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation::chirp;
using namespace constellation::message;
using namespace constellation::networking;
using namespace constellation::protocol::CHIRP;
using namespace constellation::utils;
using namespace std::chrono_literals;

std::strong_ordering RegisteredService::operator<=>(const RegisteredService& other) const {
    // Sort only by service id, we do not allow the same service on a different port
    return std::to_underlying(identifier) <=> std::to_underlying(other.identifier);
}

std::string DiscoveredService::to_uri() const {
    return ::to_uri(address, port);
}

std::strong_ordering DiscoveredService::operator<=>(const DiscoveredService& other) const {
    // Ignore IP when sorting, we only care about the host id
    const auto ord_host_id = host_id <=> other.host_id;
    // If equal, sort only by service id, we do not allow the same service on a different port
    if(std::is_eq(ord_host_id)) {
        return std::to_underlying(identifier) <=> std::to_underlying(other.identifier);
    }
    // If host id not equal, sort by host id
    return ord_host_id;
}

std::strong_ordering DiscoverCallbackEntry::operator<=>(const DiscoverCallbackEntry& other) const {
    // First sort after callback address
    const auto ord_callback =
        reinterpret_cast<std::uintptr_t>(callback) <=>    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<std::uintptr_t>(other.callback); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    // If equal, sort by service id to listen to
    if(std::is_eq(ord_callback)) {
        return std::to_underlying(service_id) <=> std::to_underlying(other.service_id);
    }
    // If callback address not equal, sort by callback address
    return ord_callback;
}

Manager::Manager(std::string_view group_name, std::string_view host_name, const std::vector<Interface>& interfaces)
    : group_id_(MD5Hash(group_name)), host_id_(MD5Hash(host_name)), logger_("LINK") {
    LOG(logger_, DEBUG) << "Host ID for satellite " << host_name << " is " << host_id_.to_string();
    LOG(logger_, DEBUG) << "Group ID for constellation " << group_name << " is " << group_id_.to_string();

    LOG(logger_, INFO) << "Using interfaces "
                       << range_to_string(interfaces, [](const auto& interface) { return interface.name; });

    const auto multicast_adddress = asio::ip::address_v4(MULTICAST_ADDRESS);
    multicast_socket_ = std::make_unique<MulticastSocket>(interfaces, multicast_adddress, PORT);
}

Manager::~Manager() {
    // First stop Run function
    main_loop_thread_.request_stop();
    if(main_loop_thread_.joinable()) {
        main_loop_thread_.join();
    }
    // Now unregister all services
    unregisterServices();
}

void Manager::start() {
    // jthread immediately starts on construction
    main_loop_thread_ = std::jthread(std::bind_front(&Manager::main_loop, this));
}

bool Manager::registerService(ServiceIdentifier service_id, Port port) {
    const RegisteredService service {service_id, port};

    std::unique_lock registered_services_lock {registered_services_mutex_};
    const auto insert_ret = registered_services_.insert(service);
    const bool actually_inserted = insert_ret.second;

    // Lock not needed anymore
    registered_services_lock.unlock();
    if(actually_inserted) {
        send_message(OFFER, service);
    }
    return actually_inserted;
}

bool Manager::unregisterService(ServiceIdentifier service_id, Port port) {
    const RegisteredService service {service_id, port};

    std::unique_lock registered_services_lock {registered_services_mutex_};
    const auto erase_ret = registered_services_.erase(service);
    const bool actually_erased = erase_ret > 0;

    // Lock not needed anymore
    registered_services_lock.unlock();
    if(actually_erased) {
        send_message(DEPART, service);
    }
    return actually_erased;
}

void Manager::unregisterServices() {
    const std::lock_guard registered_services_lock {registered_services_mutex_};
    for(auto service : registered_services_) {
        send_message(DEPART, service);
    }
    registered_services_.clear();
}

std::set<RegisteredService> Manager::getRegisteredServices() {
    const std::lock_guard registered_services_lock {registered_services_mutex_};
    return registered_services_;
}

bool Manager::registerDiscoverCallback(DiscoverCallback* callback, ServiceIdentifier service_id, std::any user_data) {
    const std::lock_guard discover_callbacks_lock {discover_callbacks_mutex_};
    const auto insert_ret = discover_callbacks_.emplace(callback, service_id, std::move(user_data));

    // Return if actually inserted
    return insert_ret.second;
}

bool Manager::unregisterDiscoverCallback(DiscoverCallback* callback, ServiceIdentifier service_id) {
    const std::lock_guard discover_callbacks_lock {discover_callbacks_mutex_};
    const auto erase_ret = discover_callbacks_.erase({callback, service_id, {}});

    // Return if actually erased
    return erase_ret > 0;
}

void Manager::unregisterDiscoverCallbacks() {
    const std::lock_guard discover_callbacks_lock {discover_callbacks_mutex_};
    discover_callbacks_.clear();
}

void Manager::forgetDiscoveredService(ServiceIdentifier identifier, message::MD5Hash host_id) {
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};
    const auto service_it = std::ranges::find_if(discovered_services_, [&](const auto& service) {
        return service.host_id == host_id && service.identifier == identifier;
    });
    if(service_it != discovered_services_.end()) {
        LOG(logger_, DEBUG) << "Dropping discovered service " << identifier << " for host id " << host_id.to_string();
        call_discover_callbacks(*service_it, ServiceStatus::DEAD);
        discovered_services_.erase(service_it);
    }
}

void Manager::forgetDiscoveredServices(message::MD5Hash host_id) {
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};

    const auto count = std::erase_if(discovered_services_, [&](const auto& service) {
        if(service.host_id == host_id) {
            call_discover_callbacks(service, ServiceStatus::DEAD);
            return true;
        }
        return false;
    });
    LOG(logger_, DEBUG) << "Dropped " << count << " discovered services for host id " << host_id.to_string();
}

void Manager::forgetDiscoveredServices() {
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};
    discovered_services_.clear();
}

std::vector<DiscoveredService> Manager::getDiscoveredServices() {
    std::vector<DiscoveredService> ret {};
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};
    std::ranges::copy(discovered_services_, std::back_inserter(ret));
    return ret;
}

std::vector<DiscoveredService> Manager::getDiscoveredServices(ServiceIdentifier service_id) {
    std::vector<DiscoveredService> ret {};
    const std::lock_guard discovered_services_lock {discovered_services_mutex_};
    for(const auto& discovered_service : discovered_services_) {
        if(discovered_service.identifier == service_id) {
            ret.push_back(discovered_service);
        }
    }
    return ret;
}

void Manager::sendRequest(ServiceIdentifier service) {
    send_message(REQUEST, {service, 0});
}

void Manager::send_message(MessageType type, RegisteredService service) {
    LOG(logger_, DEBUG) << "Sending " << type << " for " << service.identifier << " service on port " << service.port;
    const auto asm_msg = CHIRPMessage(type, group_id_, host_id_, service.identifier, service.port).assemble();
    multicast_socket_->sendMessage(asm_msg);
}

void Manager::call_discover_callbacks(const DiscoveredService& discovered_service, ServiceStatus status) {
    const std::lock_guard discover_callbacks_lock {discover_callbacks_mutex_};
    std::vector<std::future<void>> futures {};
    futures.reserve(discover_callbacks_.size());
    for(const auto& callback : discover_callbacks_) {
        if(callback.service_id == discovered_service.identifier) {
            futures.emplace_back(
                std::async(std::launch::async, callback.callback, discovered_service, status, callback.user_data));
        }
    }
    for(const auto& future : futures) {
        future.wait();
    }
}

void Manager::handle_incoming_message(message::CHIRPMessage chirp_msg, const asio::ip::address_v4& address) {
    LOG(logger_, TRACE) << "Received message from " << address.to_string()    //
                        << ": group = " << chirp_msg.getGroupID().to_string() //
                        << ", host = " << chirp_msg.getHostID().to_string()   //
                        << ", type = " << chirp_msg.getType()                 //
                        << ", service = " << chirp_msg.getServiceIdentifier() //
                        << ", port = " << chirp_msg.getPort();

    if(chirp_msg.getGroupID() != group_id_) {
        // Message from different group, ignore
        return;
    }
    if(chirp_msg.getHostID() == host_id_) {
        // Message from self, ignore
        return;
    }

    const DiscoveredService discovered_service {
        address, chirp_msg.getHostID(), chirp_msg.getServiceIdentifier(), chirp_msg.getPort()};

    switch(chirp_msg.getType()) {
    case REQUEST: {
        auto service_id = discovered_service.identifier;
        LOG(logger_, DEBUG) << "Received REQUEST for " << service_id << " services";
        const std::lock_guard registered_services_lock {registered_services_mutex_};
        // Replay OFFERs for registered services with same service identifier
        for(const auto& service : registered_services_) {
            if(service.identifier == service_id) {
                send_message(OFFER, service);
            }
        }
        break;
    }
    case OFFER: {
        std::unique_lock discovered_services_lock {discovered_services_mutex_};
        auto discovered_service_it = discovered_services_.find(discovered_service);
        if(discovered_service_it != discovered_services_.end()) {
            // Check if new port if service already discovered
            if(discovered_service_it->port != discovered_service.port) {
                // Assume old host is dead
                LOG(logger_, WARNING) << discovered_service.host_id.to_string() << " has new port "
                                      << discovered_service.port << " for " << discovered_service.identifier
                                      << " service, assuming service has been replaced";

                // Forget any discovered services of host
                discovered_services_lock.unlock();
                forgetDiscoveredServices(discovered_service.host_id);

                // Insert new service
                discovered_services_lock.lock();
                discovered_services_.insert(discovered_service);
                discovered_services_lock.unlock();
                call_discover_callbacks(discovered_service, ServiceStatus::DISCOVERED);
            }
        } else {
            discovered_services_.insert(discovered_service);

            // Unlock discovered_services_lock for user callback
            discovered_services_lock.unlock();

            LOG(logger_, DEBUG) << chirp_msg.getServiceIdentifier() << " service at " << address.to_string() << ":"
                                << chirp_msg.getPort() << " discovered";

            call_discover_callbacks(discovered_service, ServiceStatus::DISCOVERED);
        }
        break;
    }
    case DEPART: {
        std::unique_lock discovered_services_lock {discovered_services_mutex_};
        if(discovered_services_.contains(discovered_service)) {
            discovered_services_.erase(discovered_service);

            // Unlock discovered_services_lock for user callback
            discovered_services_lock.unlock();

            LOG(logger_, DEBUG) << chirp_msg.getServiceIdentifier() << " service at " << address.to_string() << ":"
                                << chirp_msg.getPort() << " departed";

            call_discover_callbacks(discovered_service, ServiceStatus::DEPARTED);
        }
        break;
    }
    default: std::unreachable();
    }
}

void Manager::main_loop(const std::stop_token& stop_token) {
    while(!stop_token.stop_requested()) {
        // Receive CHIRP message and handle it
        const auto raw_msg_opt = multicast_socket_->recvMessage(50ms);
        if(raw_msg_opt.has_value()) {
            const auto& raw_msg = raw_msg_opt.value();
            try {
                handle_incoming_message(CHIRPMessage::disassemble(raw_msg.content), raw_msg.address);
            } catch(const MessageDecodingError& error) {
                LOG(logger_, WARNING) << error.what();
                continue;
            }
        }
    }
}
