/**
 * @file
 * @brief Sender Subscription List implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QSubscriptionList.hpp"

#include <string>

#include "constellation/core/log/Level.hpp"

#include "QLogListener.hpp"

using namespace constellation::log;

QSubscriptionList::QSubscriptionList(QWidget* parent) : QWidget(parent) {
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(2);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_area_->setFrameShape(QFrame::NoFrame);

    scroll_widget_ = new QWidget();
    scroll_layout_ = new QVBoxLayout(scroll_widget_);
    scroll_layout_->setContentsMargins(6, 6, 6, 6);
    scroll_layout_->setSpacing(6);

    scroll_widget_->setLayout(scroll_layout_);
    scroll_area_->setWidget(scroll_widget_);

    layout_->addWidget(scroll_area_);
    setLayout(layout_);
}

void QSubscriptionList::addHost(const QString& name, QLogListener& log_listener, const QStringList& listItems) {

    auto* item = new QSenderSubscriptions(
        name,
        [&](const std::string& host, const std::string& topic, Level level) {
            log_listener.subscribeExtaLogTopic(host, topic, level);
        },
        [&](const std::string& host, const std::string& topic) { log_listener.unsubscribeExtraLogTopic(host, topic); },
        listItems,
        this);
    item->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    // Connect the expansion signal from sender subscription to notify_item_expanded
    connect(item, &QSenderSubscriptions::expanded, this, &QSubscriptionList::notify_item_expanded);

    items_.append(item);
    scroll_layout_->addWidget(item);
    sort_items();
}

void QSubscriptionList::removeHost(const QString& name) {
    for(auto* item : items_) {
        if(item->getName() == name) {
            // Disconnect the expanded signal to avoid dangling pointers
            disconnect(item, &QSenderSubscriptions::expanded, this, &QSubscriptionList::notify_item_expanded);

            items_.removeOne(item);
            scroll_layout_->removeWidget(item);
            delete item;
            break;
        }
    }
    sort_items();
}

void QSubscriptionList::setTopics(const QString& host, const QStringList& topics) {
    for(auto* item : items_) {
        if(item->getName() == host) {
            item->setTopics(topics);
            return;
        }
    }
}

void QSubscriptionList::notify_item_expanded(QSenderSubscriptions* expandedItem, bool expanded) {
    // If we have an expanded item and it's not the new one, collapse it
    if(expanded_item_ != nullptr && expanded_item_ != expandedItem) {
        expanded_item_->collapse();
    }

    // Only if the emitting item is expanded, store it:
    expanded_item_ = (expanded ? expandedItem : nullptr);
}

void QSubscriptionList::sort_items() {
    std::ranges::sort(items_, [](QSenderSubscriptions* a, QSenderSubscriptions* b) { return a->getName() < b->getName(); });

    // Remove all widgets from layout:
    QLayoutItem* child;
    while((child = scroll_layout_->takeAt(0)) != nullptr) {
        scroll_layout_->removeWidget(child->widget());
    }

    // Re-add in correct order:
    for(auto* item : items_) {
        scroll_layout_->addWidget(item);
    }

    // Add stretch at the end
    scroll_layout_->addStretch();
}
