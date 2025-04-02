/**
 * @file
 * @brief Sender Subscription List implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QSubscriptionList.hpp"

QSubscriptionList::QSubscriptionList(QWidget* parent) : QWidget(parent) {
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(2);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    scroll_widget_ = new QWidget();
    scroll_layout_ = new QVBoxLayout(scroll_widget_);
    scroll_layout_->setContentsMargins(0, 0, 0, 0);
    scroll_layout_->setSpacing(2);

    scroll_widget_->setLayout(scroll_layout_);
    scroll_area_->setWidget(scroll_widget_);

    layout_->addWidget(scroll_area_);
    setLayout(layout_);
}

void QSubscriptionList::addHost(const QString& name, QLogListener& log_listener, const QStringList& listItems) {

    auto* item = new QSenderSubscriptions(
        name,
        [&](const std::string& host, const std::string& topic, constellation::log::Level level) {
            log_listener.subscribeExtaLogTopic(host, topic, level);
        },
        [&](const std::string& host, const std::string& topic) { log_listener.unsubscribeExtraLogTopic(host, topic); },
        listItems,
        this);
    item->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    // Connect the expanded signal from ItemWidget to notifyItemExpanded()
    connect(item, &QSenderSubscriptions::expanded, this, &QSubscriptionList::notifyItemExpanded);

    items_.append(item);
    scroll_layout_->addWidget(item);
    sort_items();
}

void QSubscriptionList::removeHost(const QString& name) {
    for(auto* item : items_) {
        if(item->getName() == name) {
            // Disconnect the expanded signal to avoid dangling pointers
            disconnect(item, &QSenderSubscriptions::expanded, this, &QSubscriptionList::notifyItemExpanded);

            items_.removeOne(item);
            scroll_layout_->removeWidget(item);
            delete item;
            break;
        }
    }
    sort_items();
}

void QSubscriptionList::setTopics(const QString& itemName, const QStringList& topics) {
    for(auto* item : items_) {
        if(item->getName() == itemName) {
            item->setTopics(topics);
            return;
        }
    }
}

void QSubscriptionList::notifyItemExpanded(QSenderSubscriptions* expandedItem) {
    if(expanded_item_ && expanded_item_ != expandedItem) {
        expanded_item_->toggleExpand();
    }
    expanded_item_ = expandedItem;
}

void QSubscriptionList::sort_items() {
    std::sort(items_.begin(), items_.end(), [](QSenderSubscriptions* a, QSenderSubscriptions* b) {
        return a->getName() < b->getName();
    });

    for(auto* item : items_) {
        scroll_layout_->removeWidget(item);
        scroll_layout_->addWidget(item);
    }
}
