/**
 * @file
 * @brief Base pool implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "BasePool.hpp" // NOLINT(misc-header-include-cycle)

#include <algorithm>
#include <any>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/exceptions.hpp"
#include "constellation/core/networking/exceptions.hpp"
#include "constellation/core/networking/zmq_helpers.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/ManagerLocator.hpp"
#include "constellation/core/utils/thread.hpp"

namespace constellation::pools {

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::BasePool(std::string_view log_topic, std::function<void(MESSAGE&&)> callback)
        : pool_logger_(log_topic), message_callback_(std::move(callback)) {}

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::startPool() {
        // Start the pool thread
        pool_thread_ = std::jthread(std::bind_front(&BasePool::loop, this));
        utils::set_thread_name(pool_thread_, pool_logger_.getLogTopic());

        auto* chirp_manager = utils::ManagerLocator::getCHIRPManager();
        if(chirp_manager != nullptr) {
            // Call callback for all already discovered services
            const auto discovered_services = chirp_manager->getDiscoveredServices(SERVICE);
            for(const auto& discovered_service : discovered_services) {
                callback_impl(discovered_service, chirp::ServiceStatus::DISCOVERED);
            }

            // Register CHIRP callback
            chirp_manager->registerDiscoverCallback(&BasePool::callback, SERVICE, this);
            // Request currently active services
            chirp_manager->sendRequest(SERVICE);
        }
    }

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::stopPool() {
        auto* chirp_manager = utils::ManagerLocator::getCHIRPManager();
        if(chirp_manager != nullptr) {
            // Unregister CHIRP discovery callback:
            chirp_manager->unregisterDiscoverCallback(&BasePool::callback, SERVICE);
        }

        // Stop the pool thread
        pool_thread_.request_stop();
        if(pool_thread_.joinable()) {
            pool_thread_.join();
        }

        // Disconnect from all remote sockets
        disconnect_all();
    }

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    bool BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::should_connect(const chirp::DiscoveredService& /*service*/) {
        return true;
    }

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::host_connected(const chirp::DiscoveredService& /*service*/) {}

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::host_disconnected(const chirp::DiscoveredService& /*service*/) {}

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::host_disposed(const chirp::DiscoveredService& /*service*/) {}

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::checkPoolException() {
        // If exception has been thrown, disconnect from all remote sockets and propagate it
        if(exception_ptr_) {
            disconnect_all();
            std::rethrow_exception(exception_ptr_);
        }
    }

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::connect(const chirp::DiscoveredService& service) {
        using enum constellation::log::Level;

        std::unique_lock sockets_lock {sockets_mutex_};

        // Connect
        pool_logger_.log(TRACE) << "Connecting to " << service.to_uri() << "...";
        try {

            zmq::socket_t socket {*networking::global_zmq_context(), SOCKET_TYPE};
            socket.connect(service.to_uri());

            // Register the socket with the poller
            poller_.add(socket, zmq::event_flags::pollin);
            sockets_.emplace(service, std::move(socket));
            socket_count_.store(sockets_.size());
            poller_events_.resize(sockets_.size());
            pool_logger_.log(DEBUG) << "Connected to " << service.to_uri();

            // Call connected callback
            sockets_lock.unlock();
            host_connected(service);

        } catch(const zmq::error_t& error) {
            // The socket is emplaced in the list only on success of connection and poller registration and goes out of
            // scope when an exception is thrown. It calls close() automatically.
            throw networking::NetworkError("Error when registering socket for " + service.to_uri() + ": " + error.what());
        }
    }

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::disconnect_all() {
        using enum constellation::log::Level;

        std::vector<chirp::DiscoveredService> services_disconnected_ {};
        std::unique_lock sockets_lock {sockets_mutex_};

        // Unregister all sockets from the poller, then disconnect and close them.
        for(auto& [service, socket] : sockets_) {
            // Add service for callbacks
            services_disconnected_.push_back(service);

            try {
                // Remove from poller
                poller_.remove(zmq::socket_ref(socket));

                // Disconnect and close socket
                socket.disconnect(service.to_uri());
                socket.close();
            } catch(const zmq::error_t& error) {
                pool_logger_.log(DEBUG) << "Error disconnecting socket for " << service.to_uri() << ": " << error.what();
            }
        }
        sockets_.clear();
        socket_count_.store(sockets_.size());
        poller_events_.resize(sockets_.size());

        // Call disconnected callback for every host
        sockets_lock.unlock();
        for(const auto& service : services_disconnected_) {
            host_disconnected(service);
        }
    }

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::disconnect(const chirp::DiscoveredService& service) {
        using enum constellation::log::Level;

        std::unique_lock sockets_lock {sockets_mutex_};

        // Disconnect the socket
        const auto socket_it = sockets_.find(service);
        if(socket_it != sockets_.end()) {
            pool_logger_.log(TRACE) << "Disconnecting from " << service.to_uri() << "...";

            try {
                // Remove from poller
                poller_.remove(zmq::socket_ref(socket_it->second));

                // Disconnect the socket and close it
                socket_it->second.disconnect(service.to_uri());
                socket_it->second.close();
            } catch(const zmq::error_t& error) {
                pool_logger_.log(DEBUG) << "Error disconnecting socket for " << socket_it->first.to_uri() << ": "
                                        << error.what();
            }

            sockets_.erase(socket_it);
            socket_count_.store(sockets_.size());
            poller_events_.resize(sockets_.size());
            pool_logger_.log(DEBUG) << "Disconnected from " << service.to_uri();

            // Call disconnected callback
            sockets_lock.unlock();
            host_disconnected(service);
        }
    }

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::dispose(const chirp::DiscoveredService& service) {
        using enum constellation::log::Level;

        std::unique_lock sockets_lock {sockets_mutex_};

        // Disconnect the socket
        const auto socket_it = sockets_.find(service);
        if(socket_it != sockets_.end()) {
            pool_logger_.log(TRACE) << "Removing " << service.to_uri() << "...";

            try {
                // Remove from poller
                poller_.remove(zmq::socket_ref(socket_it->second));

                // Disconnect the socket and close it
                socket_it->second.disconnect(service.to_uri());
                socket_it->second.close();
            } catch(const zmq::error_t& error) {
                pool_logger_.log(DEBUG) << "Socket could not be disconnected properly for " << socket_it->first.to_uri()
                                        << ": " << error.what();
            }

            sockets_.erase(socket_it);
            socket_count_.store(sockets_.size());
            pool_logger_.log(DEBUG) << "Removed " << service.to_uri();

            // Call removed callback
            sockets_lock.unlock();
            host_disposed(service);
        }
    }

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::callback_impl(const chirp::DiscoveredService& service,
                                                                chirp::ServiceStatus status) {
        using enum constellation::log::Level;

        pool_logger_.log(TRACE) << "Callback for " << service.to_uri() << ", status " << status;

        if(status == chirp::ServiceStatus::DEPARTED) {
            disconnect(service);
        } else if(status == chirp::ServiceStatus::DISCOVERED && should_connect(service)) {
            connect(service);
        } else if(status == chirp::ServiceStatus::DEAD) {
            dispose(service);
        }
    }

    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::callback(chirp::DiscoveredService service,
                                                           chirp::ServiceStatus status,
                                                           std::any user_data) {
        auto* instance = std::any_cast<BasePool*>(user_data);
        instance->callback_impl(service, status);
    }

    template <typename MESSAGE, protocol::CHIRP::ServiceIdentifier SERVICE, zmq::socket_type SOCKET_TYPE>
    void BasePool<MESSAGE, SERVICE, SOCKET_TYPE>::loop(const std::stop_token& stop_token) {
        using enum constellation::log::Level;

        try {
            std::vector<zmq::multipart_t> polled_messages {};

            while(!stop_token.stop_requested()) {
                using namespace std::chrono_literals;

                // The poller doesn't work if no socket registered
                if(socket_count_.load() == 0) {
                    poller_event_count_.store(0);
                    std::this_thread::sleep_for(50ms);
                    continue;
                }

                std::unique_lock lock {sockets_mutex_};

                // The poller returns immediately when a socket received something, but will time out after the set period
                const auto poller_event_count = poller_.wait_all(poller_events_, 1ms);
                poller_event_count_.store(poller_event_count);

                // If no events, wait here instead of in wait_all to avoid holding the lock
                if(poller_event_count == 0) {
                    lock.unlock();
                    std::this_thread::sleep_for(50ms);
                    continue;
                }

                // Receive messages from socket
                std::for_each(poller_events_.begin(),
                              poller_events_.begin() + static_cast<std::ptrdiff_t>(poller_event_count),
                              [&polled_messages](auto& event) {
                                  // Receive multipart message
                                  zmq::multipart_t zmq_msg {};
                                  const auto received = zmq_msg.recv(event.socket);
                                  if(received) [[likely]] {
                                      polled_messages.emplace_back(std::move(zmq_msg));
                                  }
                              });

                // We don't need access to the sockets anymore, release lock
                lock.unlock();

                // Call callbacks for the polled messages
                std::ranges::for_each(polled_messages, [this](auto& zmq_msg) {
                    try {
                        message_callback_(MESSAGE::disassemble(zmq_msg));
                    } catch(const message::MessageDecodingError& error) {
                        pool_logger_.log(WARNING) << error.what();

                    } catch(const message::IncorrectMessageType& error) {
                        pool_logger_.log(WARNING) << error.what();
                    }
                });
                polled_messages.clear();
            }
        } catch(const std::exception& error) {
            pool_logger_.log(CRITICAL) << "Caught exception in pool thread: " << error.what();
            exception_ptr_ = std::current_exception();
        } catch(...) {
            pool_logger_.log(CRITICAL) << "Caught exception in pool thread";
            exception_ptr_ = std::current_exception();
        }
    }
} // namespace constellation::pools
