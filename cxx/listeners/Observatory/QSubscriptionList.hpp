/**
 * @file
 * @brief Sender Subscription List Widget
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <QList>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "QLogListener.hpp"
#include "QSenderSubscriptions.hpp"

class QSubscriptionList : public QWidget {
    Q_OBJECT

public:
    explicit QSubscriptionList(QWidget* parent = nullptr);
    void addHost(const QString& name, QLogListener& log_listener, const QStringList& listItems = {});
    void removeHost(const QString& name);

    void setTopics(const QString& host, const QStringList& topics);

private:
    void notify_item_expanded(QSenderSubscriptions* expandedItem, bool expanded);
    void sort_items();

private:
    QVBoxLayout* layout_;
    QScrollArea* scroll_area_;
    QWidget* scroll_widget_;
    QVBoxLayout* scroll_layout_;
    QList<QSenderSubscriptions*> items_;
    QSenderSubscriptions* expanded_item_ {nullptr};
};
