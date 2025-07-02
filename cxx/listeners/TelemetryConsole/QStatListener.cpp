/**
 * @file
 * @brief Stat Listener Implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QStatListener.hpp"

#include <chrono>
#include <concepts>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/core/utils/type.hpp"
#include "constellation/gui/qt_utils.hpp"
#include "constellation/listener/StatListener.hpp"

using namespace constellation::message;
using namespace constellation::gui;
using namespace constellation::listener;
using namespace constellation::utils;

QStatListener::QStatListener(QObject* parent)
    : QObject(parent), StatListener("STAT", [this](auto&& arg) { process_message(std::forward<decltype(arg)>(arg)); }) {}

QStringList QStatListener::getMetrics(const QString& sender) const {
    // Obtain list of metrics for this host:
    const auto topics = getAvailableTopics(sender.toStdString());
    QStringList metrics;
    for(const auto& [topic, desc] : topics) {
        metrics.append(QString::fromStdString(topic));
    }
    return metrics;
}

// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
void QStatListener::process_message(CMDP1StatMessage&& msg) {

    const auto sender = QString::fromStdString(std::string(msg.getHeader().getSender()));
    const auto metric = QString::fromStdString(std::string(msg.getMetric().getMetric()->name()));
    const auto unit = QString::fromStdString(std::string(msg.getMetric().getMetric()->unit()));
    const auto time = from_timepoint(msg.getHeader().getTime());

    const auto qvar = std::visit(
        [](auto&& arg) -> QVariant {
            QVariant out;
            // Qt5 cannot convert time_point, string or vectors directly
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::same_as<T, std::chrono::system_clock::time_point>) {
                out = from_timepoint(arg);
            } else if constexpr(std::is_same_v<T, std::string>) {
                out = QString::fromStdString(arg);
            } else if constexpr(is_std_vector_v<T>) {
                using U = typename T::value_type;
                QList<QVariant> vec;
                for(const auto& a : arg) {
                    if constexpr(std::same_as<U, std::chrono::system_clock::time_point>) {
                        vec.append(from_timepoint(a));
                    } else if constexpr(std::is_same_v<U, std::string>) {
                        vec.append(QString::fromStdString(a));
                    } else {
                        vec.append(QVariant::fromValue(a));
                    }
                }
                out = vec;
            } else {
                out = QVariant::fromValue(arg);
            }

            return out;
        },
        msg.getMetric().getValue());

    // Send signal
    emit newMessage(sender, metric, unit, time, qvar);
}

void QStatListener::sender_connected(std::string_view sender) {
    // Emit signals with the current number of connections & the sender name
    emit connectionsChanged(countSockets());
    emit senderConnected(QString::fromStdString(std::string(sender)));
}

void QStatListener::sender_disconnected(std::string_view sender) {
    // Emit signals with the current number of connections & the sender name
    emit connectionsChanged(countSockets());
    emit senderDisconnected(QString::fromStdString(std::string(sender)));
}

void QStatListener::topics_changed(std::string_view sender) {

    const auto s = QString::fromStdString(std::string(sender));

    // Emit signal for changed metrics
    emit metricsChanged(s, getMetrics(s));
}
