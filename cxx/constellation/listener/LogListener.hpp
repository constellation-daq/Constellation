/**
 * @file
 * @brief Subscriber pool for CMDP logging
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/build.hpp"
#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/listener/CMDPListener.hpp"

namespace constellation::listener {

    class CNSTLN_API LogListener : public CMDPListener {
    public:
        /**
         * @brief Construct log listener
         *
         * @param log_topic Logger topic to be used for this component
         * @param callback Callback function pointer for received messages
         */
        LogListener(std::string_view log_topic, std::function<void(message::CMDP1LogMessage&&)> callback);

        /**
         * @brief Set log level for global log subscription
         *
         * This subscribes to `LOG/<level>` and all higher levels.
         *
         * @param level Lowest log level to subscribe to
         */
        void setGlobalLogLevel(log::Level level);

        /**
         * @brief Get log level for global log subscription
         *
         * @return Lowest subscribed global log level
         */
        log::Level getGlobalLogLevel() const;

        /**
         * @brief Subscribe to a specific log topic
         *
         * This subscribes to `LOG/<level>/<topic>` and all higher levels.
         *
         * @note The log topic might not be empty (use `setGlobalLogLevel()` instead)
         *
         * @param log_topic Topic to subscribe to
         * @param level Lowest log level to subscribe to
         */
        void subscribeLogTopic(const std::string& log_topic, log::Level level);

        /**
         * @brief Unsubscribe from a specific log topic
         *
         * @param log_topic Topic to unsubscribe from
         */
        void unsubscribeLogTopic(const std::string& log_topic);

        /**
         * @brief Get map of subscribed log topics
         *
         * @return Map with log topics and their lowest subscribed levels
         */
        std::map<std::string, log::Level> getLogTopicSubscriptions();

        /**
         * @brief Subscribe from an extra log topic for a specific host
         *
         * This subscribes to `LOG/<level>/<topic>` and all higher levels.
         *
         * @note The log topic can be empty to set a lower generic subscription for a host
         *
         * @param host Canonical name of the host
         * @param log_topic Topic to subscribe to
         * @param level Lowest log level to subscribe to
         */
        void subscribeExtaLogTopic(const std::string& host, const std::string& log_topic, log::Level level);

        /**
         * @brief Unsubscribe from an extra log topic for a specific host
         *
         * @param host Canonical name of the host
         * @param log_topic Topic to unsubscribe from
         */
        void unsubscribeExtraLogTopic(const std::string& host, const std::string& log_topic);

        /**
         * @brief Get map of subscribed extra log topics for a specific host
         *
         * @return Map with log topics and their lowest subscribed levels
         */
        std::map<std::string, log::Level> getExtraLogTopicSubscriptions(const std::string& host);

    private:
        CNSTLN_LOCAL static std::vector<std::string> generate_topics(const std::string& log_topic,
                                                                     log::Level level,
                                                                     bool subscribe = true);
        CNSTLN_LOCAL static std::pair<std::string_view, log::Level> demangle_topic(std::string_view topic);

        // Hide subscribe/unsubscribe functions from CMDPListener
        // clang-format off
        using CMDPListener::subscribeTopic;
        using CMDPListener::unsubscribeTopic;
        using CMDPListener::multiscribeTopics;
        using CMDPListener::getTopicSubscriptions;
        using CMDPListener::subscribeExtraTopic;
        using CMDPListener::unsubscribeExtraTopic;
        using CMDPListener::multiscribeExtraTopics;
        using CMDPListener::getExtraTopicSubscriptions;
        using CMDPListener::removeExtraTopicSubscriptions;
        // clang-format on

    private:
        std::atomic<log::Level> global_log_level_;
    };

} // namespace constellation::listener
