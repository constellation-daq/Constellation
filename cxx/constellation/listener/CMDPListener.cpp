/**
 * @file
 * @brief CMDPListener implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "CMDPListener.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/utils/string.hpp"

using namespace constellation;
using namespace constellation::listener;
using namespace constellation::message;
using namespace constellation::utils;

CMDPListener::CMDPListener(std::string_view log_topic, std::function<void(CMDP1Message&&)> callback)
    : SubscriberPoolT(log_topic, [this](auto&& arg) { handle_message(std::forward<decltype(arg)>(arg)); }),
      callback_(std::move(callback)) {}

void CMDPListener::host_connected(const chirp::DiscoveredService& service) {
    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};

    // Directly subscribe to current topic list
    for(const auto& topic : subscribed_topics_) {
        SubscriberPoolT::subscribe(service.host_id, topic);
    }
    // If extra topics for host, also subscribe to those
    const auto host_it = std::ranges::find(
        extra_subscribed_topics_, service.host_id, [&](const auto& host_p) { return MD5Hash(host_p.first); });
    if(host_it != extra_subscribed_topics_.end()) {
        std::ranges::for_each(host_it->second, [&](const auto& topic) {
            if(!subscribed_topics_.contains(topic)) {
                SubscriberPoolT::subscribe(service.host_id, topic);
            }
        });
    }
}

void CMDPListener::host_disconnected(const chirp::DiscoveredService& service) {
    // Remove available topics for disconnected host
    std::unique_lock available_topics_lock {available_topics_mutex_};
    const auto topic_it =
        std::ranges::find(available_topics_, service.host_id, [&](const auto& host_p) { return MD5Hash(host_p.first); });

    // Skip if sender never connected
    if(topic_it == available_topics_.cend()) {
        return;
    }

    const auto name = topic_it->first;
    available_topics_.erase(topic_it);
    available_topics_lock.unlock();

    // Notify of disconnected sender
    LOG(BasePoolT::pool_logger_, TRACE) << "Sender " << quote(name) << " disconnected";
    sender_disconnected(name);
}

void CMDPListener::handle_message(message::CMDP1Message&& msg) {
    if(msg.isNotification()) {
        // Handle notification message:
        const auto notification = CMDP1Notification(std::move(msg));
        const auto& topics = notification.getTopics();
        const auto sender = notification.getHeader().getSender();

        bool new_topics = false;
        std::unique_lock available_topics_lock {available_topics_mutex_};
        const auto& [sender_it, new_sender] = available_topics_.try_emplace(std::string(sender));

        for(const auto& [top, desc] : topics) {
            const auto [it, inserted] = sender_it->second.insert_or_assign(top, desc.str());
            new_topics |= inserted;
        }
        available_topics_lock.unlock();

        // Call method for derived classes to propagate information
        if(new_sender) {
            LOG(BasePoolT::pool_logger_, TRACE) << "Sender " << quote(sender) << " connected";
            sender_connected(sender);
        }
        if(new_topics) {
            LOG(BasePoolT::pool_logger_, TRACE) << "Topics for " << quote(sender) << " updated";
            topics_changed(sender);
        }
    } else {
        const auto topic = msg.getTopic();
        const auto sender = msg.getHeader().getSender();

        bool new_topic = false;
        std::unique_lock available_topics_lock {available_topics_mutex_};
        const auto& [sender_it, new_sender] = available_topics_.try_emplace(std::string(sender));

        if(sender_it->second.find(topic) == sender_it->second.cend()) {
            sender_it->second.try_emplace(topic);
            new_topic = true;
        }
        available_topics_lock.unlock();

        // Call method for derived classes to propagate information
        if(new_sender) {
            LOG(BasePoolT::pool_logger_, TRACE) << "Sender " << quote(sender) << " connected";
            sender_connected(sender);
        }
        if(new_topic) {
            LOG(BasePoolT::pool_logger_, TRACE) << "Topics for " << quote(sender) << " updated";
            topics_changed(sender);
        }

        // Pass regular messages on to registered callback
        callback_(std::move(msg));
    }
}

void CMDPListener::topics_changed(std::string_view /* sender */) {}
void CMDPListener::sender_connected(std::string_view /* sender */) {}
void CMDPListener::sender_disconnected(std::string_view /* sender */) {}

std::map<std::string, std::string> CMDPListener::getAvailableTopics(std::string_view sender) const {
    const std::lock_guard topics_lock {available_topics_mutex_};
    const auto sender_it = available_topics_.find(sender);
    if(sender_it != available_topics_.cend()) {
        // Create regular map for easy consumption
        return {sender_it->second.cbegin(), sender_it->second.cend()};
    }

    return {};
}

std::map<std::string, std::string> CMDPListener::getAvailableTopics() const {
    const std::lock_guard topics_lock {available_topics_mutex_};

    std::map<std::string, std::string> topics {};
    for(const auto& [sender, sender_topics] : available_topics_) {
        std::ranges::for_each(sender_topics.cbegin(), sender_topics.cend(), [&](const auto& p) { topics.emplace(p); });
    }

    return topics;
}

std::set<std::string> CMDPListener::getAvailableSenders() const {
    const std::lock_guard topics_lock {available_topics_mutex_};

    std::set<std::string> senders {};
    std::ranges::for_each(available_topics_, [&](const auto& p) { senders.emplace(p.first); });

    return senders;
}

bool CMDPListener::isTopicAvailable(std::string_view topic) const {
    const std::lock_guard topics_lock {available_topics_mutex_};
    return std::ranges::any_of(available_topics_, [&](const auto& s) { return s.second.find(topic) != s.second.cend(); });
}

bool CMDPListener::isSenderAvailable(std::string_view sender) const {
    const std::lock_guard topics_lock {available_topics_mutex_};
    return (available_topics_.find(sender) != available_topics_.cend());
}

void CMDPListener::subscribeTopic(std::string topic) {
    multiscribeTopics({}, {std::move(topic)});
}

void CMDPListener::unsubscribeTopic(std::string topic) {
    multiscribeTopics({std::move(topic)}, {});
}

void CMDPListener::multiscribeTopics(const std::vector<std::string>& unsubscribe_topics,
                                     const std::vector<std::string>& subscribe_topics) {
    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};

    // Unsubscribe from requested topics
    std::set<std::string_view> actually_unsubscribed_topics {};
    std::ranges::for_each(unsubscribe_topics, [&](const auto& topic) {
        const auto erased = subscribed_topics_.erase(topic);
        if(erased > 0) {
            SubscriberPoolT::unsubscribe(topic);
            actually_unsubscribed_topics.emplace(topic);
        }
    });
    // Subscribe to requested topics
    std::ranges::for_each(subscribe_topics, [&](const auto& topic) {
        const auto [_, inserted] = subscribed_topics_.emplace(topic);
        if(inserted) {
            SubscriberPoolT::subscribe(topic);
        }
    });
    // Check if extra topics contained unsubscribed topics, if so subscribe again
    std::ranges::for_each(extra_subscribed_topics_, [&](const auto& host_p) {
        std::ranges::for_each(host_p.second, [&](const auto& topic) {
            if(actually_unsubscribed_topics.contains(topic)) {
                SubscriberPoolT::subscribe(host_p.first, topic);
            }
        });
    });
}

std::set<std::string> CMDPListener::getTopicSubscriptions() {
    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
    return subscribed_topics_;
}

void CMDPListener::subscribeExtraTopic(const std::string& host, std::string topic) {
    multiscribeExtraTopics(host, {}, {std::move(topic)});
}

void CMDPListener::unsubscribeExtraTopic(const std::string& host, std::string topic) {
    multiscribeExtraTopics(host, {std::move(topic)}, {});
}

void CMDPListener::multiscribeExtraTopics(const std::string& host,
                                          const std::vector<std::string>& unsubscribe_topics,
                                          const std::vector<std::string>& subscribe_topics) {
    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};

    // Check if extra topics already set
    const auto host_it = extra_subscribed_topics_.find(host);
    if(host_it == extra_subscribed_topics_.end()) {
        // Not present in map yet, subscribe to each topic and store
        std::set<std::string> topics {};
        std::ranges::for_each(subscribe_topics, [&](const auto& topic) {
            topics.emplace(topic);
            // Subscribe only if not already subscribed globally
            if(!subscribed_topics_.contains(topic)) {
                SubscriberPoolT::subscribe(host, topic);
            }
        });
        extra_subscribed_topics_.emplace(host, std::move(topics));
    } else {
        // If present in map, simply unsubscribe and subscribe while checking global subscriptions
        std::ranges::for_each(unsubscribe_topics, [&](const auto& topic) {
            const auto erased = host_it->second.erase(topic);
            // Unsubscribe only if not subscribed globally
            if(erased > 0 && !subscribed_topics_.contains(topic)) {
                SubscriberPoolT::unsubscribe(host, topic);
            }
        });
        std::ranges::for_each(subscribe_topics, [&](const auto& topic) {
            const auto [_, inserted] = host_it->second.emplace(topic);
            // Subscribe only if not already subscribed globally
            if(inserted && !subscribed_topics_.contains(topic)) {
                SubscriberPoolT::subscribe(host, topic);
            }
        });
    }
}

std::set<std::string> CMDPListener::getExtraTopicSubscriptions(const std::string& host) {
    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
    const auto host_it = extra_subscribed_topics_.find(host);
    if(host_it != extra_subscribed_topics_.end()) {
        return host_it->second;
    }
    return {};
}

void CMDPListener::removeExtraTopicSubscriptions(const std::string& host) {
    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
    const auto host_it = extra_subscribed_topics_.find(host);
    if(host_it != extra_subscribed_topics_.end()) {
        // Unsubscribe from each topic not in subscribed_topics_
        std::ranges::for_each(host_it->second, [&](const auto& topic) {
            if(!subscribed_topics_.contains(topic)) {
                SubscriberPoolT::unsubscribe(host, topic);
            }
        });
        extra_subscribed_topics_.erase(host_it);
    }
}

void CMDPListener::removeExtraTopicSubscriptions() {
    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
    std::ranges::for_each(extra_subscribed_topics_, [&](const auto& host_p) {
        // Unsubscribe from each topic not in subscribed_topics_
        std::ranges::for_each(host_p.second, [&](const auto& topic) {
            if(!subscribed_topics_.contains(topic)) {
                SubscriberPoolT::unsubscribe(host_p.first, topic);
            }
        });
    });
    extra_subscribed_topics_.clear();
}
