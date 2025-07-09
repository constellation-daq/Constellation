/**
 * @file
 * @brief Log Listener Implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QLogListener.hpp"

#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>

#include <QAbstractListModel>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/gui/QLogMessage.hpp"
#include "constellation/listener/LogListener.hpp"

using namespace constellation::message;
using namespace constellation::gui;
using namespace constellation::listener;

QLogListener::QLogListener(QObject* parent)
    : QAbstractListModel(parent),
      LogListener("MNTR", [this](auto&& arg) { add_message(std::forward<decltype(arg)>(arg)); }) {}

void QLogListener::clearMessages() {
    message_count_.store(0);
    const std::scoped_lock message_lock {message_read_mutex_, message_write_mutex_};
    if(!messages_.empty()) {
        beginRemoveRows(QModelIndex(), 0, static_cast<int>(messages_.size() - 1));
        messages_.clear();
        endRemoveRows();
    }
}

void QLogListener::add_message(CMDP1LogMessage&& msg) {

    const auto level = msg.getLogLevel();

    // Add new message to the end of the deque
    std::size_t messages_size {};
    {
        const std::lock_guard message_lock {message_write_mutex_};
        messages_.emplace_back(std::move(msg));
        messages_size = messages_.size();
    }
    message_count_.store(messages_size);

    // Call insert rows afterwards for less time with mutex
    const auto pos = static_cast<int>(messages_size - 1);
    beginInsertRows(QModelIndex(), pos, pos);
    endInsertRows();

    // Send signal
    emit newMessage(createIndex(pos, 0), level);
}

void QLogListener::sender_connected(std::string_view sender) {
    // Emit signals with the current number of connections & the sender name
    emit connectionsChanged(countSockets());
    emit senderConnected(QString::fromStdString(std::string(sender)));
}

void QLogListener::sender_disconnected(std::string_view sender) {
    // Emit signals with the current number of connections & the sender name
    emit connectionsChanged(countSockets());
    emit senderDisconnected(QString::fromStdString(std::string(sender)));
}

void QLogListener::topics_changed(std::string_view sender) {

    QStringList all_topics;

    // Fetch full list from CMDPListener
    for(const auto& [topic, desc] : getAvailableTopics()) {
        all_topics.append(QString::fromStdString(topic));
    }
    all_topics.removeDuplicates();
    all_topics.sort();

    QStringList sender_topics;
    for(const auto& [topic, desc] : getAvailableTopics(sender)) {
        sender_topics.append(QString::fromStdString(topic));
    }
    sender_topics.removeDuplicates();
    sender_topics.sort();

    // Emit signal for global topic list change:
    emit newGlobalTopics(all_topics);

    emit newSenderTopics(QString::fromStdString(std::string(sender)), sender_topics);
}

QVariant QLogListener::data(const QModelIndex& index, int role) const {
    if(role != Qt::DisplayRole || !index.isValid()) {
        return {};
    }

    if(index.column() < QLogMessage::countColumns()) {
        const std::shared_lock message_lock {message_read_mutex_};
        if(index.row() < static_cast<int>(messages_.size())) {
            return messages_[index.row()][index.column()];
        }
    }

    return {};
}

const QLogMessage& QLogListener::getMessage(const QModelIndex& index) const {
    const std::shared_lock message_lock {message_read_mutex_};
    return messages_[index.row()];
}

QVariant QLogListener::headerData(int column, Qt::Orientation orientation, int role) const {

    if(role == Qt::DisplayRole && orientation == Qt::Horizontal && column >= 0 && column < QLogMessage::countColumns()) {
        return QLogMessage::columnName(column);
    }

    return {};
}
