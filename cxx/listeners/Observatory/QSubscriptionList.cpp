/**
 * @file
 * @brief Sender Subscription List implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QSubscriptionList.hpp"

#include <algorithm>
#include <memory>
#include <string>

#include "constellation/core/log/Level.hpp"

#include "QLogListener.hpp"
#include "QSenderSubscriptions.hpp"

using namespace constellation::log;

QSubscriptionList::QSubscriptionList(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(2);
    setLayout(layout_);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    layout_->addWidget(scroll_area_);

    scroll_widget_ = new QWidget(this);
    // FIXME deduce this width from parent widget - somehow that always returns 100px?
    scroll_widget_->setFixedWidth(266);
    scroll_area_->setWidget(scroll_widget_);

    scroll_layout_ = new QVBoxLayout(scroll_widget_);
    scroll_layout_->setContentsMargins(6, 6, 6, 6);
    scroll_layout_->setSpacing(6);
}

void QSubscriptionList::addHost(const QString& host, QLogListener& log_listener, const QStringList& listItems) {

    auto item = std::make_shared<QSenderSubscriptions>(
        host,
        [&](const std::string& host, const std::string& topic, Level level) {
            log_listener.subscribeExtaLogTopic(host, topic, level);
        },
        [&](const std::string& host, const std::string& topic) { log_listener.unsubscribeExtraLogTopic(host, topic); },
        listItems,
        this);
    item->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    // Connect the expansion signal from sender subscription to notify_item_expanded
    connect(item.get(), &QSenderSubscriptions::expanded, this, &QSubscriptionList::notify_item_expanded);

    items_.emplace_back(item);
    scroll_layout_->addWidget(item.get());

    // Re-build layout
    rebuild_layout();
}

void QSubscriptionList::removeHost(const QString& host) {
    auto item = std::ranges::find_if(items_, [host](const auto& it) { return it->getName() == host; });
    if(item == items_.end()) {
        return;
    }

    // Disconnect the expanded signal to avoid dangling pointers
    disconnect(item->get(), &QSenderSubscriptions::expanded, this, &QSubscriptionList::notify_item_expanded);

    // Remove from layout and delete
    scroll_layout_->removeWidget(item->get());
    items_.erase(item);

    // Re-build layout
    rebuild_layout();
}

void QSubscriptionList::setTopics(const QString& host, const QStringList& topics) {
    auto item = std::ranges::find_if(items_, [host](const auto& it) { return it->getName() == host; });
    if(item != items_.end()) {
        (*item)->setTopics(topics);
    }
}

void QSubscriptionList::notify_item_expanded(QSenderSubscriptions* item, bool expanded) {
    // If we have an expanded item and it's not the new one, collapse it
    if(expanded_item_ != nullptr && expanded_item_ != item) {
        expanded_item_->collapse();
    }

    // Only if the emitting item is expanded, store it:
    expanded_item_ = (expanded ? item : nullptr);
}

void QSubscriptionList::rebuild_layout() {

    // Remove all widgets from layout:
    QLayoutItem* child = nullptr;
    while((child = scroll_layout_->takeAt(0)) != nullptr) {
        if(child->widget() != nullptr) {
            scroll_layout_->removeWidget(child->widget());
        }
    }

    // Sort entries
    std::ranges::sort(items_, [](const auto& a, const auto& b) { return a->getName() < b->getName(); });

    // Re-add in correct order:
    std::ranges::for_each(items_, [&](const auto& it) { scroll_layout_->addWidget(it.get()); });

    // Add stretch at the end
    scroll_layout_->addStretch();
}
