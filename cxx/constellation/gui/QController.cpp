/**
 * @file
 * @brief QController implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QController.hpp"

#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <utility>

#include <zmq.hpp>

#include <QAbstractListModel>
#include <QFont>
#include <QMetaType>
#include <QSortFilterProxyModel>
#include <Qt>

#include "constellation/controller/Controller.hpp"
#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/gui/qt_utils.hpp"

using namespace constellation::config;
using namespace constellation::controller;
using namespace constellation::gui;
using namespace constellation::message;
using namespace constellation::protocol;
using namespace constellation::utils;

QController::QController(std::string controller_name, QObject* parent)
    : QAbstractListModel(parent), Controller(std::move(controller_name)) {}

int QController::rowCount(const QModelIndex& /*unused*/) const {
    return static_cast<int>(getConnectionCount());
}

int QController::columnCount(const QModelIndex& /*unused*/) const {
    return static_cast<int>(headers_.size());
}

QVariant QController::data(const QModelIndex& index, int role) const {

    if(!index.isValid()) {
        return {};
    }

    if(index.row() >= static_cast<int>(getConnectionCount()) || index.column() >= static_cast<int>(headers_.size())) {
        return {};
    }

    const std::lock_guard connection_lock {connection_mutex_};
    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    return get_data(it, index.column(), role);
}

QVariant QController::get_data(std::map<std::string, Connection, std::less<>>::const_iterator connection,
                               std::size_t idx,
                               int role) {

    const auto& name = connection->first;
    const auto& conn = connection->second;

    if(role == Qt::DecorationRole && (idx == 3 || idx == 9)) {
        return get_response_icon(conn.last_cmd_type);
    }

    if(role == Qt::ForegroundRole) {
        if(idx == 2) {
            return get_state_color(conn.state);
        }
    }

    if(role == Qt::BackgroundRole) {
        if((idx == 4 || idx == 5) && conn.lives < 3) {
            const auto alpha = (3 - conn.lives) * 85;
            return QColor(255, 0, 0, alpha);
        }
    }

    if(role == Qt::FontRole && idx == 2) {
        QFont font;
        font.setBold(true);
        return font;
    }

    // Below only handle display role
    if(role != Qt::DisplayRole) {
        return {};
    }

    switch(idx) {
    case 0: {
        // Satellite type
        const auto type_endpos = name.find_first_of('.', 0);
        return QString::fromStdString(name.substr(0, type_endpos));
    }
    case 1: {
        // Satellite name
        const auto name_startpos = name.find_first_of('.', 0);
        return QString::fromStdString(name.substr(name_startpos + 1));
    }
    case 2: {
        // State
        return get_state_string(conn.state, true);
    }
    case 3: {
        // Last command response message
        return QString::fromStdString(conn.last_message);
    }
    case 4: {
        // Heartbeat period
        return QString::fromStdString(to_string(conn.interval));
    }
    case 5: {
        // Remaining lives:
        return conn.lives;
    }
    case 6: {
        // Connection (URI)
        try {
            const std::string last_endpoint = conn.req.get(zmq::sockopt::last_endpoint);
            return QString::fromStdString(last_endpoint);
        } catch(const zmq::error_t& e) {
            return QString::fromStdString(e.what());
        }
    }
    case 7: {
        // MD5 host ID
        return QString::fromStdString(conn.host_id.to_string());
    }
    case 8: {
        // Role
        return QString::fromStdString(enum_name(conn.role));
    }
    case 9: {
        // Last command response type
        return QString::fromStdString(utils::enum_name(conn.last_cmd_type));
    }
    case 10: {
        // Last heartbeat
        return from_timepoint(conn.last_heartbeat);
    }
    case 11: {
        // Last checked
        return from_timepoint(conn.last_checked);
    }
    default: {
        return QString("");
    }
    }
}

QMap<QString, QVariant> QController::getQDetails(const QModelIndex& index) const {

    if(!index.isValid() || index.row() >= static_cast<int>(getConnectionCount())) {
        return {};
    }

    const std::lock_guard connection_lock {connection_mutex_};
    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    QMap<QString, QVariant> details;
    for(std::size_t i = 0; i < headers_.size(); i++) {
        details.insert(headers_.at(i), get_data(it, i));
    }
    for(std::size_t i = 0; i < headers_details_.size(); i++) {
        details.insert(headers_details_.at(i), get_data(it, i + headers_.size()));
    }

    return details;
}

QVariant QController::headerData(int column, Qt::Orientation orientation, int role) const {
    if(role == Qt::DisplayRole && orientation == Qt::Horizontal && column >= 0 &&
       column < static_cast<int>(headers_.size())) {
        return QString::fromStdString(headers_.at(column));
    }
    return {};
}

void QController::reached_state(CSCP::State state, bool global) {
    LOG(logger_, DEBUG) << "Reached new " << (global ? "global" : "lowest") << " state " << state;
    emit reachedState(state, global);
}

void QController::leaving_state(CSCP::State state, bool global) {
    emit leavingState(state, global);
}

void QController::propagate_update(UpdateType type, std::size_t position, std::size_t total) {
    if(type == UpdateType::ADDED) {
        beginInsertRows(QModelIndex(), static_cast<int>(position), static_cast<int>(position));
        endInsertRows();
        // Emit signal for changed connections
        emit connectionsChanged(total);
    } else if(type == UpdateType::REMOVED) {
        beginRemoveRows(QModelIndex(), static_cast<int>(position), static_cast<int>(position));
        endRemoveRows();

        // Emit signal for changed connections
        emit connectionsChanged(total);
    }

    emit dataChanged(createIndex(static_cast<int>(position), 0),
                     createIndex(static_cast<int>(position), headers_.size() - 1));
}

Dictionary QController::getQCommands(const QModelIndex& index) {
    const std::lock_guard lock {connection_mutex_};

    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    return it->second.commands;
}

std::string QController::getQName(const QModelIndex& index) const {
    const std::lock_guard lock {connection_mutex_};

    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    return it->first;
}

CSCP1Message QController::sendQCommand(const QModelIndex& index, const std::string& verb, const CommandPayload& payload) {
    std::unique_lock<std::mutex> lock {connection_mutex_};

    // Select connection by index:
    auto it = connections_.begin();
    std::advance(it, index.row());

    // Unlock so the controller can grab it
    lock.unlock();

    auto msg = Controller::sendCommand(it->first, verb, payload);
    emit dataChanged(createIndex(index.row(), 0), createIndex(index.row(), headers_.size() - 1));
    return msg;
}

std::map<std::string, CSCP1Message> QController::sendQCommands(std::string verb, const CommandPayload& payload) {
    auto replies = sendCommands(std::move(verb), payload);

    emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(getConnectionCount() - 1), headers_.size() - 1));
    return replies;
}

std::map<std::string, CSCP1Message> QController::sendQCommands(const std::string& verb,
                                                               const std::map<std::string, CommandPayload>& payloads) {
    auto replies = sendCommands(verb, payloads);

    emit dataChanged(createIndex(0, 0), createIndex(static_cast<int>(getConnectionCount() - 1), headers_.size() - 1));
    return replies;
}

QControllerSortProxy::QControllerSortProxy(QObject* parent) : QSortFilterProxyModel(parent) {}

bool QControllerSortProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const {

    const QVariant leftData = sourceModel()->data(left);
    const QVariant rightData = sourceModel()->data(right);

    const QString leftString = leftData.toString();
    const QString rightString = rightData.toString();
    return QString::localeAwareCompare(leftString, rightString) < 0;
}
