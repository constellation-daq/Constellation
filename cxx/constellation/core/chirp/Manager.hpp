/**
 * @file
 * @brief CHIRP Manager
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <any>
#include <compare>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <asio/ip/address_v4.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/MulticastSocket.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/networking/asio_helpers.hpp"
#include "constellation/core/networking/Port.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"

namespace constellation::chirp {

    /** A service offered by the host and announced by the `Manager` */
    struct RegisteredService {
        /** Service identifier of the offered service */
        protocol::CHIRP::ServiceIdentifier identifier;

        /** Port of the offered service */
        networking::Port port;

        CNSTLN_API std::strong_ordering operator<=>(const RegisteredService& other) const;
    };

    /** A service discovered by the `Manager` */
    struct DiscoveredService {
        /** Address of the discovered service */
        asio::ip::address_v4 address;

        /** Host ID of the discovered service */
        message::MD5Hash host_id;

        /** Service identifier of the discovered service */
        protocol::CHIRP::ServiceIdentifier identifier;

        /** Port of the discovered service */
        networking::Port port;

        /** Convert service information to a URI */
        CNSTLN_API std::string to_uri() const;

        CNSTLN_API std::strong_ordering operator<=>(const DiscoveredService& other) const;
    };

    /** Status of a service for callbacks from the `Manager` */
    enum class ServiceStatus : std::uint8_t {
        /** The service is newly discovered */
        DISCOVERED,
        /** The service departed */
        DEPARTED,
        /** The service is considered dead without departure */
        DEAD,
    };

    /**
     * Function signature for user callback
     *
     * The first argument (``service``) contains the discovered service, the second argument is an enum that indicates the
     * status of the service, i.e. whether it is newly discovered, departing, or considered dead, and the third argument
     * (``user_data``) is arbitrary user data passed to the callback (done via `Manager::RegisterDiscoverCallback`).
     *
     * It is recommended to pass the user data wrapped in an atomic `std::shared_ptr` since the callback is
     * launched asynchronously in a detached `std::thread`. If the data is modified, it is recommended to use
     * atomic types when possible or a `std::mutex` for locking to ensure thread-safe access.
     */
    using DiscoverCallback = void(DiscoveredService service, ServiceStatus status, std::any user_data);

    /** Entry for a user callback in the `Manager` for newly discovered or departing services */
    struct DiscoverCallbackEntry {
        /** Function pointer to a callback */
        DiscoverCallback* callback;

        /** Service identifier of the service for which callbacks should be received */
        protocol::CHIRP::ServiceIdentifier service_id;

        /**
         * Arbitrary user data passed to the callback function
         *
         * For information on lifetime and thread-safety see `DiscoverCallback`.
         */
        std::any user_data;

        CNSTLN_API std::strong_ordering operator<=>(const DiscoverCallbackEntry& other) const;
    };

    /** Manager handling CHIRP messages */
    class Manager {
    public:
        /**
         * @param group_name Group name of the group to join
         * @param host_name Host name for outgoing messages
         * @param interfaces Interfaces to use
         */
        CNSTLN_API Manager(std::string_view group_name,
                           std::string_view host_name,
                           const std::vector<networking::Interface>& interfaces);

        CNSTLN_API virtual ~Manager();

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        Manager(const Manager& other) = delete;
        Manager& operator=(const Manager& other) = delete;
        Manager(Manager&& other) = delete;
        Manager& operator=(Manager&& other) = delete;
        /// @endcond

        /**
         * @return Group ID (MD5Hash of the group name)
         */
        constexpr message::MD5Hash getGroupID() const { return group_id_; }

        /**
         * @return Host ID (MD5Hash of the host name)
         */
        constexpr message::MD5Hash getHostID() const { return host_id_; }

        /** Start the background thread of the manager */
        CNSTLN_API void start();

        /**
         * Register a service offered by the host in the manager
         *
         * Calling this function sends a CHIRP message with OFFER type, and registers the service such that the manager
         * responds to CHIRP messages with REQUEST type and the corresponding service identifier.
         *
         * @param service_id Service identifier of the offered service
         * @param port Port of the offered service
         * @retval true if the service was registered
         * @retval false if the service was already registered
         */
        CNSTLN_API bool registerService(protocol::CHIRP::ServiceIdentifier service_id, networking::Port port);

        /**
         * Unregister a previously registered service offered by the host in the manager
         *
         * Calling this function sends a CHIRP message with DEPART type and removes the service from manager. See also
         * `RegisterService`.
         *
         * @param service_id Service identifier of the previously offered service
         * @param port Port of the previously offered service
         * @retval true If the service was unregistered
         * @retval false If the service was never registered
         */
        CNSTLN_API bool unregisterService(protocol::CHIRP::ServiceIdentifier service_id, networking::Port port);

        /**
         * Unregisters all offered services registered in the manager
         *
         * Equivalent to calling `UnregisterService` for every registered service.
         */
        CNSTLN_API void unregisterServices();

        /**
         * Get the list of services currently registered in the manager
         *
         * @return Set with all currently registered services
         */
        CNSTLN_API std::set<RegisteredService> getRegisteredServices();

        /**
         * Register a user callback for newly discovered or departing services
         *
         * Note that a callback function can be registered multiple times for different services.
         *
         * @warning Discover callbacks block the execution of further processing of CHIRP requests and offers, callbacks that
         *          take a long time should offload the work to a new thread.
         *
         * @param callback Function pointer to a callback
         * @param service_id Service identifier of the service for which callbacks should be received
         * @param user_data Arbitrary user data passed to the callback function (see `DiscoverCallback`)
         * @retval true If the callback/service/user_data combination was registered
         * @retval false If the callback/service/user_data combination was already registered
         */
        CNSTLN_API bool registerDiscoverCallback(DiscoverCallback* callback,
                                                 protocol::CHIRP::ServiceIdentifier service_id,
                                                 std::any user_data);

        /**
         * Unregister a previously registered callback for newly discovered or departing services
         *
         * @param callback Function pointer to the callback of registered callback entry
         * @param service_id Service identifier of registered callback entry
         * @retval true If the callback entry was unregistered
         * @retval false If the callback entry was never registered
         */
        CNSTLN_API bool unregisterDiscoverCallback(DiscoverCallback* callback,
                                                   protocol::CHIRP::ServiceIdentifier service_id);

        /**
         * Unregisters all discovery callbacks registered in the manager
         *
         * Equivalent to calling `UnregisterDiscoverCallback` for every discovery callback.
         */
        CNSTLN_API void unregisterDiscoverCallbacks();

        /**
         * @brief Forget the previously discovered services of the given type and host ID, if present
         *
         * @param identifier Identifier of the discovered service to be forgotten
         * @param host_id Host ID of the discovered service to be forgotten
         */
        CNSTLN_API void forgetDiscoveredService(protocol::CHIRP::ServiceIdentifier identifier, message::MD5Hash host_id);

        /**
         * @brief Forget all previously discovered services of a given host
         *
         * @param host_id Host ID of the discovered services to be forgotten
         */
        CNSTLN_API void forgetDiscoveredServices(message::MD5Hash host_id);

        /** Forget all previously discovered services */
        CNSTLN_API void forgetDiscoveredServices();

        /**
         * Returns list of all discovered services
         *
         * @return Vector with all discovered services
         */
        CNSTLN_API std::vector<DiscoveredService> getDiscoveredServices();

        /**
         * Returns list of all discovered services with a given service identifier
         *
         * @param service_id Service identifier for discovered services that should be listed
         * @return Vector with all discovered services with the given service identifier
         */
        CNSTLN_API std::vector<DiscoveredService> getDiscoveredServices(protocol::CHIRP::ServiceIdentifier service_id);

        /**
         * Send a discovery request for a specific service identifier
         *
         * This sends a CHIRP message with a REQUEST type and a given service identifier. Other hosts might reply with a
         * CHIRP message with OFFER type for the given service identifier. These can be retrieved either by registering a
         * user callback (see `RegisterDiscoverCallback`) or by getting the list of discovered services shortly
         * after (see `GetDiscoveredServices`).
         *
         * @param service_id Service identifier to send a request for
         */
        CNSTLN_API void sendRequest(protocol::CHIRP::ServiceIdentifier service_id);

    private:
        /**
         * Send a CHIRP message
         *
         * @param type CHIRP message type
         * @param service Service with identifier and port
         */
        void send_message(protocol::CHIRP::MessageType type, RegisteredService service);

        /**
         * Call all discover callbacks
         */
        void call_discover_callbacks(const DiscoveredService& discovered_service, ServiceStatus status);

        /**
         * Responds to incoming CHIRP message with REQUEST type by sending CHIRP messages with OFFER type for all registered
         * services. It also tracks incoming CHIRP messages with OFFER and DEPART type to form the list of discovered
         * services and calls the corresponding discovery callbacks.
         */
        void handle_incoming_message(message::CHIRPMessage chirp_msg, const asio::ip::address_v4& address);

        /**
         * Main loop listening and handling incoming CHIRP messages
         *
         * @param stop_token Token to stop loop via `std::jthread`
         */
        void main_loop(const std::stop_token& stop_token);

    private:
        std::unique_ptr<MulticastSocket> multicast_socket_;

        message::MD5Hash group_id_;
        message::MD5Hash host_id_;

        log::Logger logger_;

        /** Set of registered services */
        std::set<RegisteredService> registered_services_;

        /** Mutex for thread-safe access to `registered_services_` */
        std::mutex registered_services_mutex_;

        /** Set of discovered services */
        std::set<DiscoveredService> discovered_services_;

        /** Mutex for thread-safe access to `discovered_services_` */
        std::mutex discovered_services_mutex_;

        /** Set of discovery callbacks */
        std::set<DiscoverCallbackEntry> discover_callbacks_;

        /** Mutex for thread-safe access to `discover_callbacks_` */
        std::mutex discover_callbacks_mutex_;

        std::jthread main_loop_thread_;
    };

} // namespace constellation::chirp
