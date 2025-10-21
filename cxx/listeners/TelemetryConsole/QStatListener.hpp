/**
 * @file
 * @brief Qt Wrapper for Stat Listener
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <string_view>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "constellation/core/message/CMDP1Message.hpp"
#include "constellation/listener/StatListener.hpp"

/**
 * @class QStatListener
 * @brief Qt Wrapper around the StatListener
 * @details This wrapper class implements a callback function for CMDP message reception which re-emits the message as
 *          Qt signal such that UI element slots can be directly connected. The telemetry message value is converted into a
 *          QVariant for display. For Qt5 compatibility, this is explicitly done for timestamps, vectors and strings.
 */
class QStatListener : public QObject, public constellation::listener::StatListener {
    Q_OBJECT

public:
    /**
     * @brief Constructor of QStatListener
     *
     * @param parent QObject parent
     */
    explicit QStatListener(QObject* parent = nullptr);

    /**
     * @brief Obtain list of current metric topics for a sender
     *
     * @param sender Name of the sender
     * @return List of available metric topics
     */
    QStringList getMetrics(const QString& sender) const;

signals:
    /**
     * @brief Signal emitted whenever a connection changed
     * @param connections Number of currently held connections
     */
    void connectionsChanged(std::size_t connections);

    /**
     * @brief Signal emitted when a new sender connects to the stat listener
     * @param host Canonical name of the newly connected sender
     */
    void senderConnected(const QString& host);

    /**
     * @brief Signal emitted when a sender disconnects from the stat listener
     * @param host Canonical name of the disconnected sender
     */
    void senderDisconnected(const QString& host);

    /**
     * @brief Signal emitted when the registered metrics for a given host changed
     * @param host Canonical name of the sender
     * @param metrics List of metrics registered for this host
     */
    void metricsChanged(const QString& host, const QStringList& metrics);

    /**
     * @brief Signal emitted whenever a new stat message has been received
     * @param sender Name of the sending host
     * @param metric Name of the metric
     * @param unit Unit of the metric
     * @param time Timestamp of the sender
     * @param value Value of the metric
     */
    void newMessage(
        const QString& sender, const QString& metric, const QString& unit, const QDateTime& time, const QVariant& value);

private:
    /**
     * @brief Callback registered for receiving stat messages from the subscription pool
     *
     * @param msg Received stat message
     */
    void process_message(constellation::message::CMDP1StatMessage&& msg);

    /**
     * @brief Helper callback to emit connected signals
     * @param sender Canonical name of the sender that connected
     */
    void sender_connected(std::string_view sender) override;

    /**
     * @brief Helper callback to emit disconnected signals
     * @param sender Canonical name of the sender that disconnected
     */
    void sender_disconnected(std::string_view sender) override;

    /**
     * @brief Helper to emit signals for update metric topics
     * @param sender CCanonical name of the sender that sent the topic notification
     */
    void topics_changed(std::string_view sender) override;
};
