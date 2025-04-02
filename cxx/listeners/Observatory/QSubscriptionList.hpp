/**
 * @file
 * @brief Sender Subscription List Widget
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <map>
#include <memory>
#include <string>

#include <QList>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "constellation/core/log/Level.hpp"

#include "QLogListener.hpp"
#include "QSenderSubscriptions.hpp"

class QSubscriptionList : public QWidget {
    Q_OBJECT

public:
    explicit QSubscriptionList(QWidget* parent = nullptr);
    void addHost(const QString& name, const QStringList& listItems = {});
    void removeHost(const QString& name);

    // NEW: Function to add an item to an existing ItemWidget
    void setTopics(const QString& host, const QString& topics);

    void notifyItemExpanded(QSenderSubscriptions* expandedItem);

private:
    QVBoxLayout* m_layout;
    QScrollArea* m_scrollArea;
    QWidget* m_scrollWidget;
    QVBoxLayout* m_scrollLayout;
    QList<QSenderSubscriptions*> m_items;
    QSenderSubscriptions* m_currentExpandedItem;

    void sort_items();
};
