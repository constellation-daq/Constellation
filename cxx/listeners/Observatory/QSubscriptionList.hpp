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

#include <QListWidget>

#include "constellation/core/log/Level.hpp"

#include "QLogListener.hpp"
#include "QSenderSubscriptions.hpp"

class QSubscriptionList : public QListWidget {
    Q_OBJECT

public:
    QSubscriptionList(QWidget* parent = nullptr) : QListWidget(parent) {
        setStyleSheet("QListWidget { background-color: transparent; }");
        setAutoFillBackground(false);
        setResizeMode(QListView::Adjust);
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        setUniformItemSizes(false);
    }
    virtual ~QSubscriptionList() = default;

    // No copy constructor/assignment/move constructor/assignment
    /// @cond doxygen_suppress
    QSubscriptionList(const QSubscriptionList& other) = delete;
    QSubscriptionList& operator=(const QSubscriptionList& other) = delete;
    QSubscriptionList(QSubscriptionList&& other) noexcept = delete;
    QSubscriptionList& operator=(QSubscriptionList&& other) = delete;
    /// @endcond

    void setTopics(const QString& host, const QStringList& topics) {
        auto it = senders_.find(host);
        if(it != senders_.end()) {
            auto* widget = dynamic_cast<QSenderSubscriptions*>(itemWidget(it->second.get()));
            widget->setTopics(topics);
        }
    }

    void emplace(const QString& host, QLogListener& log_listener) {
        // Make new QListWidgetItem and store it
        auto list_item = std::make_shared<QListWidgetItem>();
        senders_.emplace(host, list_item);

        // Generate widget and add it to the QListWidgetItem - QListWidget takes ownership
        auto* widget = new QSenderSubscriptions(
            this,
            host,
            [&](const std::string& host, const std::string& topic, constellation::log::Level level) {
                log_listener.subscribeExtaLogTopic(host, topic, level);
            },
            [&](const std::string& host, const std::string& topic) { log_listener.unsubscribeExtraLogTopic(host, topic); });
        list_item->setSizeHint(widget->sizeHint());

        // Add to QListWidget:
        addItem(list_item.get());
        setItemWidget(list_item.get(), widget);

        // Listen for size changes
        connect(widget, &QSenderSubscriptions::sizeChanged, this, [list_item, widget]() {
            list_item->setSizeHint(widget->sizeHint());
        });
    }
    void erase(const QString& host) {
        auto it = senders_.find(host);
        if(it != senders_.end()) {
            removeItemWidget(it->second.get());
            senders_.erase(it);
        }
    }

private:
    std::map<QString, std::shared_ptr<QListWidgetItem>> senders_;
};
