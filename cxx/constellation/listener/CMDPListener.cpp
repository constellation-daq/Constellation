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
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "constellation/core/chirp/Manager.hpp"
#include "constellation/core/message/CHIRPMessage.hpp"
#include "constellation/core/message/CMDP1Message.hpp"

using namespace constellation;
using namespace constellation::listener;
using namespace constellation::message;

CMDPListener::CMDPListener(std::string_view log_topic, std::function<void(CMDP1Message&&)> callback)
    : SubscriberPoolT(log_topic, std::move(callback)) {}

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

void CMDPListener::set_topic_subscriptions(std::set<std::string> topics) {
    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};

    // Set of topics to unsubscribe: current topics not in new topics
    std::set<std::string_view> to_unsubscribe {};
    std::ranges::for_each(subscribed_topics_, [&](const auto& topic) {
        if(!topics.contains(topic)) {
            to_unsubscribe.emplace(topic);
        }
    });
    // Set of topics to subscribe: new topics not in current topics
    std::set<std::string_view> to_subscribe {};
    std::ranges::for_each(topics, [&](const auto& new_topic) {
        if(!subscribed_topics_.contains(new_topic)) {
            to_subscribe.emplace(new_topic);
        }
    });
    // Unsubscribe from old topics
    std::ranges::for_each(to_unsubscribe, [&](const auto& topic) { SubscriberPoolT::unsubscribe(topic); });
    // Subscribe to new topics
    std::ranges::for_each(to_subscribe, [&](const auto& topic) { SubscriberPoolT::subscribe(topic); });

    // Check if extra topics contained unsubscribed topics, if so subscribe again
    std::ranges::for_each(extra_subscribed_topics_, [&](const auto& host_p) {
        std::ranges::for_each(host_p.second, [&](const auto& topic) {
            if(to_unsubscribe.contains(topic)) {
                SubscriberPoolT::subscribe(host_p.first, topic);
            }
        });
    });

    // Store new set of subscribed topics
    subscribed_topics_ = std::move(topics);
}

void CMDPListener::subscribeTopic(std::string topic) {
    multiscribeTopics({}, {std::move(topic)});
}

void CMDPListener::unsubscribeTopic(std::string topic) {
    multiscribeTopics({std::move(topic)}, {});
}

void CMDPListener::multiscribeTopics(const std::vector<std::string>& unsubscribe_topics,
                                     std::vector<std::string> subscribe_topics) {
    std::set<std::string> new_subscribed_topics {};
    {
        // Copy current topics
        const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
        new_subscribed_topics = subscribed_topics_;
    }
    // Erase requested topics
    std::size_t changed = 0;
    for(const auto& topic : unsubscribe_topics) {
        changed += new_subscribed_topics.erase(topic);
    }
    // Emplace new topics
    for(auto& topic : std::move(subscribe_topics)) {
        const auto insert_res = new_subscribed_topics.emplace(std::move(topic));
        changed += static_cast<std::size_t>(insert_res.second);
    }
    // Handle logic in set_topic_subscriptions
    if(changed > 0) [[likely]] {
        set_topic_subscriptions(std::move(new_subscribed_topics));
    }
}

std::set<std::string> CMDPListener::getTopicSubscriptions() {
    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
    return subscribed_topics_;
}

void CMDPListener::set_extra_topic_subscriptions(const std::string& host, std::set<std::string> topics) {
    const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};

    // Check if extra topics already set
    const auto host_it = extra_subscribed_topics_.find(host);
    if(host_it != extra_subscribed_topics_.end()) {
        // Set of topics to unsubscribe: current topics not in subscribed_topics or new topics
        std::set<std::string_view> to_unsubscribe {};
        std::ranges::for_each(host_it->second, [&](const auto& topic) {
            if(!subscribed_topics_.contains(topic) && !topics.contains(topic)) {
                to_unsubscribe.emplace(topic);
            }
        });
        // Set of topics to subscribe: new topics not in subscribed_topics and current topics
        std::set<std::string_view> to_subscribe {};
        std::ranges::for_each(topics, [&](const auto& new_topic) {
            if(!subscribed_topics_.contains(new_topic) && !host_it->second.contains(new_topic)) {
                to_subscribe.emplace(new_topic);
            }
        });
        // Unsubscribe from old topics
        std::ranges::for_each(to_unsubscribe, [&](const auto& topic) { SubscriberPoolT::unsubscribe(host, topic); });
        // Subscribe to new topics
        std::ranges::for_each(to_subscribe, [&](const auto& topic) { SubscriberPoolT::subscribe(host, topic); });
    } else {
        // Subscribe to each topic not present in subscribed_topics_
        std::ranges::for_each(topics, [&](const auto& topic) {
            if(!subscribed_topics_.contains(topic)) {
                SubscriberPoolT::subscribe(host, topic);
            }
        });
    }

    // Store new set of extra topics
    extra_subscribed_topics_[host] = std::move(topics);
}

void CMDPListener::subscribeExtraTopic(const std::string& host, std::string topic) {
    multiscribeExtraTopics(host, {}, {std::move(topic)});
}

void CMDPListener::unsubscribeExtraTopic(const std::string& host, std::string topic) {
    multiscribeExtraTopics(host, {std::move(topic)}, {});
}

void CMDPListener::multiscribeExtraTopics(const std::string& host,
                                          const std::vector<std::string>& unsubscribe_topics,
                                          std::vector<std::string> subscribe_topics) {
    std::set<std::string> new_subscribed_topics {};
    {
        const std::lock_guard subscribed_topics_lock {subscribed_topics_mutex_};
        const auto host_it = extra_subscribed_topics_.find(host);
        if(host_it != extra_subscribed_topics_.end()) {
            // Copy current topics
            new_subscribed_topics = host_it->second;
        }
    }
    // Erase requested topics
    std::size_t changed = 0;
    for(const auto& topic : unsubscribe_topics) {
        changed += new_subscribed_topics.erase(topic);
    }
    // Emplace new topics
    for(auto& topic : std::move(subscribe_topics)) {
        const auto insert_res = new_subscribed_topics.emplace(std::move(topic));
        changed += static_cast<std::size_t>(insert_res.second);
    }

    // Handle logic in set_extra_topic_subscriptions
    if(changed > 0) [[likely]] {
        set_extra_topic_subscriptions(host, std::move(new_subscribed_topics));
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
