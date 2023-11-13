#pragma once

#include <any>
#include <mutex>
#include <set>
#include <string_view>
#include <thread>
#include <vector>

#include "asio.hpp"

#include "CHIRP/config.hpp"
#include "CHIRP/BroadcastRecv.hpp"
#include "CHIRP/BroadcastSend.hpp"
#include "CHIRP/Message.hpp"

namespace cnstln {
namespace CHIRP {

/**
 * A service offered by the host
*/
struct RegisteredService {
    /** Service identifier of the offered service */
    ServiceIdentifier identifier;

    /** Port of the offered service */
    Port port;

    CHIRP_API bool operator<(const RegisteredService& other) const;
};

/**
 * A service discovered by the :cpp:class:`Manager`
 */
struct DiscoveredService {
    /** Address of the discovered service */
    asio::ip::address address;

    /** Host ID of the discovered service */
    MD5Hash host_id;

    /** Service identifier of the discovered service */
    ServiceIdentifier identifier;

    /** Port of the discovered service */
    Port port;

    CHIRP_API bool operator<(const DiscoveredService& other) const;
};

/**
 * Function signature for user callback
 *
 * Note: the callback has to be thread-safe.
*/
using DiscoverCallback = void(DiscoveredService service, bool depart, std::any user_data);

/**
 * Unique entry for a user callback for newly discovered and departing services
*/
struct DiscoverCallbackEntry {
    /** Function pointer to a callback */
    DiscoverCallback* callback;

    /** Service identifier of the service for which callbacks should be received */
    ServiceIdentifier service_id;

    /** Arbitrary user data passed to the callback function */
    std::any user_data;

    CHIRP_API bool operator<(const DiscoverCallbackEntry& other) const;
};

/**
 * Manager for CHIRP broadcasting and receiving
 */
class Manager {
public:
    /**
     * @param brd_address Broadcast address for outgoing broadcast messages
     * @param any_address Any address for incoming broadcast messages
     * @param group_name Group name of the group to join
     * @param host_name Host name for outgoing messages
    */
    CHIRP_API Manager(asio::ip::address brd_address, asio::ip::address any_address, std::string_view group_name, std::string_view host_name);

    /**
     * @param brd_ip Broadcast IP for outgoing broadcast messages
     * @param any_ip Any IP for incoming broadcast messages
     * @param group_name Group name of the group to join
     * @param host_name Host name for outgoing messages
    */
    CHIRP_API Manager(std::string_view brd_ip, std::string_view any_ip, std::string_view group_name, std::string_view host_name);

    /** */
    CHIRP_API virtual ~Manager();

    /**
     * Get the group ID
     *
     * @return MD5Hash of the group name
    */
    constexpr MD5Hash GetGroupID() const { return group_hash_; }

    /**
     * Get the host ID
     *
     * @return MD5Hash of the host name
    */
    constexpr MD5Hash GetHostID() const { return host_hash_; }

    /**
     * Start the background thread of the manager
    */
    CHIRP_API void Start();

    /**
     * Register a service offered by the host in the manager
     *
     * Calling this function sends a CHIRP broadcast with OFFER type, and registers the service such that the manager
     * responds to CHIRP broadcasts with REQUEST type and the corresponding service identifier.
     *
     * @param service_id Service identifier of the offered service
     * @param port Port of the offered service
     * @retval true if the service was registered
     * @retval false if the service was already registered
    */
    CHIRP_API bool RegisterService(ServiceIdentifier service_id, Port port);

    /**
     * Unregister a previously registered service offered by the host in the manager
     *
     * Calling this function sends a CHIRP broadcast with DEPART type and removes the service from manager. See also
     * :cpp:func:`RegisterService`.
     *
     * @param service_id Service identifier of the previously offered service
     * @param port Port of the previously offered service
     * @retval true If the service was unregistered
     * @retval false If the service was never registered
    */
    CHIRP_API bool UnregisterService(ServiceIdentifier service_id, Port port);

    /**
     * Unregisteres all offered services registered in the manager
     *
     * Equivalent to calling :cpp:func:`UnregisterService` for every registered service.
    */
    CHIRP_API void UnregisterServices();

    /**
     * Get the list of services currently registered in the manager
     *
     * @return Set with all currently registered services
    */
    CHIRP_API std::set<RegisteredService> GetRegisteredServices();

    /**
     * Register a user callback for newly discovered or departing servies
     *
     * Note that a callback function can be registered multiple times for different servies.
     *
     * @param callback Function pointer to a callback
     * @param service_id Service identifier of the service for which callbacks should be received
     * @param user_data Arbitrary user data passed to the callback function
     * @retval true If the callback/service/user_data combination was registered
     * @retval false If the callback/service/user_data combination was already registered
    */
    CHIRP_API bool RegisterDiscoverCallback(DiscoverCallback* callback, ServiceIdentifier service_id, std::any user_data);

    /**
     * Unegister a previously registered callback for newly discovered or departing services
     *
     * @param callback Function pointer to the callback of registered callback entry
     * @param service_id Service identifier of registered callback entry
     * @retval true If the callback entry was unregistered
     * @retval false If the callback entry was never registered
    */
    CHIRP_API bool UnregisterDiscoverCallback(DiscoverCallback* callback, ServiceIdentifier service_id);

    /**
     * Unregisteres all discovery callbacks registered in the manager
     *
     * Equivalent to calling :cpp:func:`UnregisterDiscoverCallback` for every discovery callback.
    */
    CHIRP_API void UnregisterDiscoverCallbacks();

    /**
     * Forgets all previously discovered services
    */
    CHIRP_API void ForgetDiscoveredServices();

    /**
     * Returns list of all discovered services
     *
     * @return Vector with all discovered services
    */
    CHIRP_API std::vector<DiscoveredService> GetDiscoveredServices();

    /**
     * Returns list of all discovered services with a given service identifier
     *
     * @param service_id Service identifier for discovered services that should be listed
     * @return Vector with all discovered services with the given service identifier
    */
    CHIRP_API std::vector<DiscoveredService> GetDiscoveredServices(ServiceIdentifier service_id);

    /**
     * Send a discovery request for a specific service identifier
     *
     * This sends a CHIRP broadcast with a REQUEST type and a given service identifier. Other hosts might reply with a
     * CHIRP broadcast with OFFER type for the given service identifier. These can be retrieved either by registering a user
     * callback (see :cpp:func:`RegisterDiscoverCallback`) or by getting the list of discovered services shortly after
     * (see :cpp:func:`GetDiscoveredServices`).
     *
     * @param service_id Service identifier to send a request for
    */
    CHIRP_API void SendRequest(ServiceIdentifier service_id);

private:
    /**
     * Send a CHIRP broadcast
     *
     * @param type CHIRP broadcast message type
     * @param service Service with identifier and port
    */
    void SendMessage(MessageType type, RegisteredService service);

    /**
     * Run loop listening and responding to incoming CHIRP broadcasts
     *
     * The run loop responds to incoming CHIRP broadcasts with REQUEST type by sending CHIRP broadcasts with OFFER type for
     * all registered servies. It also tracks incoming CHIRP broadcasts with OFFER and DEPART type to form the list of
     * discovered services and calls the corresponding discovery callbacks.
     *
     * @param stop_token Token to stop loop via :cpp:class:`std::jthread`
    */
    void Run(std::stop_token stop_token);

private:
    BroadcastRecv receiver_;
    BroadcastSend sender_;

    /** MD5 hash of the group name, representing the CHIRP group ID */
    MD5Hash group_hash_;

    /** MD5 hash of the host name, representing the CHIRP host ID */
    MD5Hash host_hash_;

    /** Set of registered services */
    std::set<RegisteredService> registered_services_;

    /** Mutex for thread-safe access to :cpp:member:`registered_services_` */
    std::mutex registered_services_mutex_;

    /** Set of discovered services */
    std::set<DiscoveredService> discovered_services_;

    /** Mutex for thread-safe access to :cpp:member:`discovered_services_` */
    std::mutex discovered_services_mutex_;

    /** Set of discovery callbacks */
    std::set<DiscoverCallbackEntry> discover_callbacks_;

    /** Mutex for thread-safe access to :cpp:member:`discover_callbacks_` */
    std::mutex discover_callbacks_mutex_;

    std::jthread run_thread_;
};

}
}
