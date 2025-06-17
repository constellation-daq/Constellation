/**
 * @file
 * @brief Subscriber pool for CMDP
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/pools/SubscriberPool.hpp"
#include "constellation/core/protocol/CHIRP_definitions.hpp"
#include "constellation/core/utils/string_hash_map.hpp"

namespace constellation::listener {

    class CNSTLN_API CMDPListener
        : public pools::SubscriberPool<message::CMDP1Message, protocol::CHIRP::ServiceIdentifier::MONITORING> {
    public:
        using SubscriberPoolT = pools::SubscriberPool<message::CMDP1Message, protocol::CHIRP::ServiceIdentifier::MONITORING>;

        /**
         * @brief Construct CMDPListener
         *
         * @param log_topic Logger topic to be used for this component
         * @param callback Callback function pointer for received messages
         */
        CMDPListener(std::string_view log_topic, std::function<void(message::CMDP1Message&&)> callback);

        // No copy/move constructor/assignment
        /// @cond doxygen_suppress
        CMDPListener(const CMDPListener& other) = delete;
        CMDPListener& operator=(const CMDPListener& other) = delete;
        CMDPListener(CMDPListener&& other) noexcept = delete;
        CMDPListener& operator=(CMDPListener&& other) = delete;
        /// @endcond

        virtual ~CMDPListener() = default;

        /**
         * @brief Subscribe to a given topic for all sockets
         *
         * @param topic Topic to subscribe to
         */
        void subscribeTopic(std::string topic);

        /**
         * @brief Unsubscribe from a given topic for all sockets
         *
         * @param topic Topic to unsubscribe from
         */
        void unsubscribeTopic(std::string topic);

        /**
         * @brief Unsubscribe from and subscribe to multiple topics for all sockets
         *
         * @param unsubscribe_topics List of topics to unsubscribe from
         * @param subscribe_topics List of topics to subscribe to
         */
        void multiscribeTopics(const std::vector<std::string>& unsubscribe_topics,
                               const std::vector<std::string>& subscribe_topics);

        /**
         * @brief Get set of subscribed topics for all sockets
         *
         * @return Set containing the currently subscribed topics
         */
        std::set<std::string> getTopicSubscriptions();

        /**
         * @brief Subscribe to a given topic for a specific socket
         *
         * @note Extra topics are topics subscribed to in addition to the topics for every socket
         *
         * @param host Canonical name of the host to subscribe to
         * @param topic Topic to subscribe to
         */
        void subscribeExtraTopic(const std::string& host, std::string topic);

        /**
         * @brief Unsubscribe from a given topic for a specific socket
         *
         * @note Only unsubscribes if not in topics that every socket is subscribed to
         *
         * @param host Canonical name of the host to unsubscribe from
         * @param topic Topic to unsubscribe from
         */
        void unsubscribeExtraTopic(const std::string& host, std::string topic);

        /**
         * @brief Unsubscribe from and subscribe to multiple extra topics for a specific socket
         *
         * @param host Canonical name of the host to unsubscribe from and subscribe to
         * @param unsubscribe_topics List of topics to unsubscribe from
         * @param subscribe_topics List of topics to subscribe to
         */
        void multiscribeExtraTopics(const std::string& host,
                                    const std::vector<std::string>& unsubscribe_topics,
                                    const std::vector<std::string>& subscribe_topics);

        /**
         * @brief Get set of subscribed extra topics for a specific socket
         *
         * @note Extra topics are topics subscribed to in addition to the topics for every socket
         *
         * @return Set containing the currently subscribed extra topics for given host
         */
        std::set<std::string> getExtraTopicSubscriptions(const std::string& host);

        /**
         * @brief Remove extra topics for a specific socket
         *
         * @param host Canonical name of the host
         */
        void removeExtraTopicSubscriptions(const std::string& host);

        /**
         * @brief Remove extra topics for all sockets
         */
        void removeExtraTopicSubscriptions();

        /**
         * @brief Obtain available topics for given sender. Topics are parsed from CMDP notification messages and cached per
         * sender. Returns an empty map if the sender is not known or has not sent a topic notification yet.
         *
         * @param sender Sending CMDP host to get available topics for
         * @return Map with available topics as keys and their description as values
         */
        std::map<std::string, std::string> getAvailableTopics(std::string_view sender) const;

        /**
         * @brief Obtain all available topics from any sender.
         *
         * @return Map with available topics as keys and their description as values
         */
        std::map<std::string, std::string> getAvailableTopics() const;

        /**
         * @brief Obtain all available senders.
         *
         * @return Set with available senders
         */
        std::set<std::string> getAvailableSenders() const;

        /**
         * @brief Check if a given topic is in the list of available topics
         *
         * @note the comparison here is case-sensitive.
         *
         * @param topic Topic to search for
         * @return Boolean indicating whether the topic is available or not
         */
        bool isTopicAvailable(std::string_view topic) const;

        /**
         * @brief Check if a given sender is known and available
         *
         * @note the comparison here is case-sensitive.
         *
         * @param sender Sender to search for
         * @return Boolean indicating whether the sender is available or not
         */
        bool isSenderAvailable(std::string_view sender) const;

    protected:
        /**
         * @brief Method for derived classes to act on newly connected sockets
         *
         * @warning Derived functions should always call `CMDPListener::host_connected()` to ensure that sockets are
         *          subscribed to the correct topics.
         */
        void host_connected(const chirp::DiscoveredService& service) override;

        /**
         * @brief Method for derived classes to act on sockets before disconnecting
         *
         * @warning Derived functions should always call `CMDPListener::host_disconnected()`
         */
        void host_disconnected(const chirp::DiscoveredService& service) override;

        /**
         * @brief Method for derived classes to act on topic notifications
         *
         * @param sender CMDP sending host of the topic notification
         */
        virtual void topics_changed(std::string_view sender);

        /**
         * @brief Method for derived classes to act on new senders
         *
         * @param sender New CMDP sending host
         */
        virtual void sender_connected(std::string_view sender);

        /**
         * @brief Method for derived classes to act on disconnecting senders
         *
         * @param sender Disconnected CMDP sending host
         */
        virtual void sender_disconnected(std::string_view sender);

    private:
        /**
         * @brief Helper methods to separate notification messages from regular CMDP messages. Notifications are handled
         * internally while regular messages are passed on to the provided callback of the implementing class.
         *
         * @param msg CMDP message to process
         */
        void handle_message(message::CMDP1Message&& msg);

    private:
        // Hide subscribe/unsubscribe functions from SubscriberPool
        using SubscriberPoolT::subscribe;
        using SubscriberPoolT::unsubscribe;

    private:
        /* Callback */
        std::function<void(message::CMDP1Message&&)> callback_;

        /* Subscribed topics */
        std::mutex subscribed_topics_mutex_;
        std::set<std::string> subscribed_topics_;
        utils::string_hash_map<std::set<std::string>> extra_subscribed_topics_;

        /* Available topics from notification */
        mutable std::mutex available_topics_mutex_;
        utils::string_hash_map<utils::string_hash_map<std::string>> available_topics_;
    };

} // namespace constellation::listener
