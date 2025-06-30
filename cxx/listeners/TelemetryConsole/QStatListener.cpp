/**
 * @file
 * @brief Stat Listener Implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QStatListener.hpp"

#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/gui/qt_utils.hpp"
#include "constellation/listener/LogListener.hpp"

using namespace constellation::message;
using namespace constellation::gui;
using namespace constellation::listener;

QStatListener::QStatListener(QObject* parent)
    : QObject(parent), StatListener("STAT", [this](auto&& arg) { process_message(std::forward<decltype(arg)>(arg)); }) {}

void QStatListener::process_message(CMDP1StatMessage&& msg) {

    const auto sender = QString::fromStdString(std::string(msg.getHeader().getSender()));
    const auto metric = QString::fromStdString(std::string(msg.getMetric().getMetric()->name()));
    const auto time = from_timepoint(msg.getHeader().getTime());

    const auto qvar =
        std::visit([](auto&& arg) -> QVariant { return QVariant::fromValue(arg); }, msg.getMetric().getValue());

    // Send signal
    emit newMessage(sender, metric, time, qvar);
}

void QStatListener::sender_connected(std::string_view sender) {
    // Emit signals with the current number of connections & the sender name
    emit senderConnected(QString::fromStdString(std::string(sender)));
}

void QStatListener::sender_disconnected(std::string_view sender) {
    // Emit signals with the current number of connections & the sender name
    emit senderDisconnected(QString::fromStdString(std::string(sender)));
}
