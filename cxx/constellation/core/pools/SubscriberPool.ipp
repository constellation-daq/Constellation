/**
 * @file
 * @brief Subscriber pool implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "SubscriberPool.hpp"

#include <functional>
#include <initializer_list>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include <zmq.hpp>

#include "constellation/core/chirp/CHIRP_definitions.hpp"

namespace constellation::pools {

    template <typename MESSAGE>
    SubscriberPool<MESSAGE>::SubscriberPool(chirp::ServiceIdentifier service,
                                            std::string_view log_topic,
                                            std::function<void(const MESSAGE&)> callback,
                                            std::initializer_list<std::string> default_topics)
        : BasePool<MESSAGE>(service, log_topic, std::move(callback), zmq::socket_type::sub),
          default_topics_(default_topics) {}

    template <typename MESSAGE>
    void SubscriberPool<MESSAGE>::scribe(std::string_view host, std::string_view topic, bool subscribe) {
        // Get host ID from name:
        const auto host_id = message::MD5Hash(host);

        const std::lock_guard sockets_lock {BasePool<MESSAGE>::sockets_mutex_};

        const auto socket_it = std::ranges::find_if(
            BasePool<MESSAGE>::get_sockets(), host_id, [&](const auto& s) { return s.first.host_id == host_id; });
        if(socket_it != BasePool<MESSAGE>::get_sockets().end()) {
            if(subscribe) {
                socket_it->second.subscribe(topic);
            } else {
                socket_it->second.unsubscribe(topic);
            }
        }
    }

    template <typename MESSAGE> void SubscriberPool<MESSAGE>::socket_connected(zmq::socket_t& socket) {
        // Directly subscribe to default topic list
        for(const auto& topic : default_topics_) {
            socket.set(zmq::sockopt::subscribe, topic);
        }
    }

    template <typename MESSAGE> void SubscriberPool<MESSAGE>::subscribe(std::string_view host, std::string_view topic) {
        scribe(host, topic, true);
    }

    template <typename MESSAGE> void SubscriberPool<MESSAGE>::unsubscribe(std::string_view host, std::string_view topic) {
        scribe(host, topic, false);
    }

} // namespace constellation::pools
