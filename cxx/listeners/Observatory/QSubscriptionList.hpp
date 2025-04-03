/**
 * @file
 * @brief Sender Subscription List Widget
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>
#include <vector>

#include <QList>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "QLogListener.hpp"
#include "QSenderSubscriptions.hpp"

/**
 * @class QSubscriptionList
 * @brief List of host subscriptions for display in a UI
 */
class QSubscriptionList : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     *
     * @param parent Parent widget
     */
    explicit QSubscriptionList(QWidget* parent = nullptr);

    /**
     * @brief Add new host to the subscription list
     *
     * @param host Canonical name of the host
     * @param log_listener Reference to the log listener class to generate callbacks for
     * @param topics Optional initial list of topics available for subscription
     */
    void addHost(const QString& host, QLogListener& log_listener, const QStringList& topics = {});

    /**
     * @brief Remove a host from the subscription list
     *
     * @param host Canonical name of the host to be removed
     */
    void removeHost(const QString& host);

    /**
     * @brief Set the available log topics for this host
     *
     * This clears previously-listed topics and sets the newly provided list
     *
     * @param host Host to set topics for
     * @param topics List of available topics
     */
    void setTopics(const QString& host, const QStringList& topics);

private:
    /**
     * @brief Helper to collapse other list sections when a new one was opened
     *
     * @param item Pointer to the item which has emitted the signal
     * @param expanded Flag whether the emitting item was expanded or collapsed
     */
    void notify_item_expanded(QSenderSubscriptions* item, bool expanded);

    /**
     * @brief Helper to rebuild the layout after hosts have been added or removed
     */
    void rebuild_layout();

private:
    QVBoxLayout* layout_;
    QScrollArea* scroll_area_;
    QWidget* scroll_widget_;
    QVBoxLayout* scroll_layout_;

    // List of host subscription entries
    std::vector<std::shared_ptr<QSenderSubscriptions>> items_;
    QSenderSubscriptions* expanded_item_ {nullptr};
};
