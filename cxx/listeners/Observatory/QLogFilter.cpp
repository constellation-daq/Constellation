/**
 * @file
 * @brief Log Filter Implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QLogFilter.hpp"

#include <string>
#include <utility>

#include <QModelIndex>
#include <QObject>
#include <QSortFilterProxyModel>
#include <QString>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/gui/QLogMessage.hpp"

#include "QLogListener.hpp"

using namespace constellation::log;
using namespace constellation::utils;

QLogFilter::QLogFilter(QObject* parent) : QSortFilterProxyModel(parent), logger_("MNTR"), filter_level_(Level::WARNING) {
    // Make filtering case-insensitive
    filter_message_.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
}

bool QLogFilter::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    const auto src_index = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto* listener = dynamic_cast<QLogListener*>(sourceModel());
    const auto& msg = listener->getMessage(src_index);

    const auto match = filter_message_.match(QString::fromStdString(std::string(msg.getLogMessage())));
    return (msg.getLogLevel() >= filter_level_) &&
           (msg.getHeader().getSender() == filter_sender_ || "- All -" == filter_sender_) &&
           (msg.getLogTopic() == filter_topic_ || "- All -" == filter_topic_) && match.hasMatch();
}

void QLogFilter::setFilterLevel(Level level) {
    if(filter_level_ != level) {
        LOG(logger_, DEBUG) << "Updating filter level to " << to_string(level);
        filter_level_ = level;
        invalidateFilter();
    }
}

void QLogFilter::setFilterSender(std::string sender) {
    LOG(logger_, DEBUG) << "Updating filter sender to " << sender;
    filter_sender_ = std::move(sender);
    invalidateFilter();
}

void QLogFilter::setFilterTopic(std::string topic) {
    LOG(logger_, DEBUG) << "Updating filter topic to " << topic;
    filter_topic_ = std::move(topic);
    invalidateFilter();
}

void QLogFilter::setFilterMessage(const QString& pattern) {
    LOG(logger_, DEBUG) << "Updating filter pattern for message to " << pattern.toStdString();
    filter_message_.setPattern(pattern);
    invalidateFilter();
}
