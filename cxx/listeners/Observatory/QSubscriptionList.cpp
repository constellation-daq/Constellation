/**
 * @file
 * @brief Sender Subscription List implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QSubscriptionList.hpp"

QSubscriptionList::QSubscriptionList(QWidget* parent) : QWidget(parent), m_currentExpandedItem(nullptr) {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(2);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_scrollWidget = new QWidget();
    m_scrollLayout = new QVBoxLayout(m_scrollWidget);
    m_scrollLayout->setContentsMargins(0, 0, 0, 0);
    m_scrollLayout->setSpacing(2);

    m_scrollWidget->setLayout(m_scrollLayout);
    m_scrollArea->setWidget(m_scrollWidget);

    m_layout->addWidget(m_scrollArea);
    setLayout(m_layout);
}

void QSubscriptionList::addHost(const QString& name, const QStringList& listItems) {
    auto* item = new QSenderSubscriptions(name, listItems, this);
    item->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    // Connect the expanded signal from ItemWidget to notifyItemExpanded()
    connect(item, &QSenderSubscriptions::expanded, this, &QSubscriptionList::notifyItemExpanded);

    m_items.append(item);
    m_scrollLayout->addWidget(item);
    sort_items();
}

void QSubscriptionList::removeHost(const QString& name) {
    for(auto* item : m_items) {
        if(item->getName() == name) {
            // Disconnect the expanded signal to avoid dangling pointers
            disconnect(item, &QSenderSubscriptions::expanded, this, &QSubscriptionList::notifyItemExpanded);

            m_items.removeOne(item);
            m_scrollLayout->removeWidget(item);
            delete item;
            break;
        }
    }
    sort_items();
}

void QSubscriptionList::setTopics(const QString& itemName, const QString& listItem) {
    for(auto* item : m_items) {
        if(item->getName() == itemName) {
            item->addListItem(listItem); // Now only QSubscriptionList can call this
            return;
        }
    }
}

void QSubscriptionList::notifyItemExpanded(QSenderSubscriptions* expandedItem) {
    if(m_currentExpandedItem && m_currentExpandedItem != expandedItem) {
        m_currentExpandedItem->toggleExpand();
    }
    m_currentExpandedItem = expandedItem;
}

void QSubscriptionList::sort_items() {
    std::sort(m_items.begin(), m_items.end(), [](QSenderSubscriptions* a, QSenderSubscriptions* b) {
        return a->getName() < b->getName();
    });

    for(auto* item : m_items) {
        m_scrollLayout->removeWidget(item);
        m_scrollLayout->addWidget(item);
    }
}
