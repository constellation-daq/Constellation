/**
 * @file
 * @brief Abstract base pool
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
     * Abstract Base pool class
     *
     * This class registers a CHIRP callback for the services defined via the template parameter, listens to incoming
     * messages and forwards them to a callback registered upon creation of the socket
     */
    template <typename MESSAGE> class BasePool {
    public:
        /**
         * @brief Construct BasePool
         *
         * @param service CHIRP service identifier for which a subscription should be made
         * @param logger Reference to a logger to be used for this component
         * @param callback Callback function pointer for received messages
         */
        BasePool(chirp::ServiceIdentifier service, const log::Logger& logger, std::function<void(const MESSAGE&)> callback);

        /**
         * @brief Destruct BasePool
         *
         * This closes all connections and unregisters the CHIRP service discovery callback
         */
        virtual ~BasePool();

        // No copy/move constructor/assignment
        BasePool(const BasePool& other) = delete;
        BasePool& operator=(const BasePool& other) = delete;
        BasePool(BasePool&& other) = delete;
        BasePool& operator=(BasePool&& other) = delete;

        /**
         * @brief Callback for CHIRP service discovery
         * @details This callback is run for every service of the given type discovered on or departing from the
         * constellation. It will connect or disconnect the remote service and register the corresponding socket
         * with the central polling mechanism
         *
         * @param service Discovered service
         * @param depart Boolean to indicate discovery or departure
         * @param user_data Pointer to the BasePool instance
         */
        static void callback(chirp::DiscoveredService service, bool depart, std::any user_data);

        /**
         * @brief Method for derived classes to act on newly connected sockets
         *
         * @param socket The newly connected socket
         */
        virtual void socket_connected(zmq::socket_t& /*unused*/) {};

        /**
         * @brief Method for derived classes to act on sockets before disconnecting
         *
         * @param socket The socket to be disconnected
         */
        virtual void socket_disconnected(zmq::socket_t& /*unused*/) {};

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

        const log::Logger& logger_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        zmq::context_t context_;
        zmq::active_poller_t poller_;

    protected:
        std::map<chirp::DiscoveredService, zmq::socket_t> sockets_;
        std::timed_mutex sockets_mutex_;

    private:
        std::atomic_flag af_;
        std::jthread pool_thread_;

        std::function<void(const MESSAGE&)> message_callback_;
    };
} // namespace constellation::utils

// Include template members
#include "BasePool.tpp"
