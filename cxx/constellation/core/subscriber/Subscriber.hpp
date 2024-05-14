/**
 * @file
 * @brief Abstract receiver
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */
#pragma once

#include <any>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <stop_token>
#include <thread>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/logging/Logger.hpp"
#include "constellation/core/message/CHP1Message.hpp"
#include "constellation/core/message/exceptions.hpp"

namespace constellation::utils {

    /**
     * Abstract Subscriber class
     *
     * This class registers a CHIRP callback for the services defined via the template parameter, listens to incoming
     * messages and forwards them to a callback registered upon creation of the Subscriber
     */
    template <typename MESSAGE> class Subscriber {
    public:
        /**
         * @brief Construct Subscriber
         *
         * @param callback Callback function pointer for received messages
         */
        CNSTLN_API Subscriber(chirp::ServiceIdentifier service,
                              const std::string& logger_name,
                              std::function<void(const MESSAGE&)> callback);

        /**
         * @brief Destruct Subscriber
         *
         * This closes all connections and unregisters the CHIRP service discovery callback
         */
        CNSTLN_API virtual ~Subscriber();

        // No copy/move constructor/assignment
        Subscriber(const Subscriber& other) = delete;
        Subscriber& operator=(const Subscriber& other) = delete;
        Subscriber(Subscriber&& other) = delete;
        Subscriber& operator=(Subscriber&& other) = delete;

        /**
         * @brief Callback for CHIRP service discovery
         * @details This callback is run for every service of the given type discovered on or departing from the
         * constellation. It will connect or disconnect the remote service and register the corresponding socket
         * with the central polling mechanism
         *
         * @param service Discovered service
         * @param depart Boolean to indicate discovery or departure
         * @param user_data Pointer to the Subscriber instance
         */
        CNSTLN_API static void callback(chirp::DiscoveredService service, bool depart, std::any user_data);

        void subscribe(std::string_view host, std::string_view topic);
        void unsubscribe(std::string_view host, std::string_view topic);

    private:
        /** Callback implementation */
        void callback_impl(const chirp::DiscoveredService& service, bool depart);

        /** Main loop polling the registered sockets */
        void loop(const std::stop_token& stop_token);

        /** Helper to connect to a newly discovered socket */
        void connect(const chirp::DiscoveredService& service);

        /** Helper to disconnect from a departing service */
        void disconnect(const chirp::DiscoveredService& service);

        /** Disconnect from all registered services */
        void disconnect_all();

        chirp::ServiceIdentifier service_;

        log::Logger logger_;
        zmq::context_t context_;
        zmq::active_poller_t poller_;
        std::map<chirp::DiscoveredService, zmq::socket_t> sockets_;

        std::timed_mutex sockets_mutex_;
        std::atomic_flag af_;
        std::jthread subscriber_thread_;

        std::function<void(const MESSAGE&)> message_callback_;
    };
} // namespace constellation::utils

// Include template members
#include "Subscriber.tpp"