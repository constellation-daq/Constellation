/**
 * @file
 * @brief Abstract subscriber pool
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <functional>
#include <initializer_list>
#include <set>
#include <string>
#include <string_view>

#include "constellation/core/chirp/CHIRP_definitions.hpp"
#include "constellation/core/pools/BasePool.hpp"

#include "zmq.hpp"

namespace constellation::pools {

    /**
     * Abstract Subscriber pool class
     *
     * This class registers a CHIRP callback for the services defined via the template parameter, listens to incoming
     * messages and forwards them to a callback registered upon creation of the subscriber socket
     */
    template <typename MESSAGE, chirp::ServiceIdentifier SERVICE>
    class SubscriberPool : public BasePool<MESSAGE, SERVICE, zmq::socket_type::sub> {
    public:
        using BasePoolT = BasePool<MESSAGE, SERVICE, zmq::socket_type::sub>;

        /**
         * @brief Construct SubscriberPool
         *
         * @param log_topic Logger topic to be used for this component
         * @param callback Callback function pointer for received messages
         * @param default_topics List of default subscription topics to which this component subscribes directly upon
         *        opening the socket
         */
        SubscriberPool(std::string_view log_topic,
                       std::function<void(MESSAGE&&)> callback,
                       std::initializer_list<std::string> default_topics = {});

        /**
         * @brief Subscribe to a given topic of a specific host
         *
         * @param host Canonical name of the host to subscribe to
         * @param topic Topic to subscribe to
         */
        void subscribe(std::string_view host, std::string_view topic);

        /**
         * @brief Unsubscribe from a given topic of a specific host
         *
         * @param host Canonical name of the host to unsubscribe from
         * @param topic Topic to unsubscribe
         */
        void unsubscribe(std::string_view host, std::string_view topic);

    private:
        void socket_connected(zmq::socket_t& socket) override;

        /** Sub- or unsubscribe to a topic */
        void scribe(std::string_view host, std::string_view topic, bool subscribe);
        std::set<std::string> default_topics_;
    };
} // namespace constellation::pools

// Include template members
#include "SubscriberPool.ipp" // IWYU pragma: keep
