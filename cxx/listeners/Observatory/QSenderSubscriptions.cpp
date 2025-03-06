/**
 * @file
 * @brief Sender Subscription Implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QSenderSubscriptions.hpp"

#include <functional>

#include "constellation/core/log/Level.hpp"

#include "ui_QSenderSubscriptions.h"

using namespace constellation;

QSenderSubscriptions::QSenderSubscriptions(
    QWidget* parent,
    const QString& name,
    std::function<void(const std::string&, const std::string&, log::Level)> sub_callback,
    std::function<void(const std::string&, const std::string&)> unsub_callback)
    : QWidget(parent), name_(name), ui_(new Ui::QSenderSubscriptions), sub_callback_(std::move(sub_callback)),
      unsub_callback_(std::move(unsub_callback)) {
    ui_->setupUi(this);
    ui_->senderName->setText(name_);
    ui_->topicsView->setModel(&topics_);

    // Connect selection change from topics list:
    connect(ui_->topicsView->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            [&](const QItemSelection& selected, const QItemSelection& deselected) {
                for(const auto& idx : selected.indexes()) {
                    const auto topic = topics_.itemFromIndex(idx)->text().toStdString();
                    sub_callback_(name_.toStdString(), topic, log::Level::TRACE);
                }
                for(const auto& idx : deselected.indexes()) {
                    const auto topic = topics_.itemFromIndex(idx)->text().toStdString();
                    unsub_callback_(name_.toStdString(), topic);
                }
            });
}

void QSenderSubscriptions::on_senderLevel_currentIndexChanged(int index) {
    const auto lvl = log::Level(index);
    sub_callback_(name_.toStdString(), "", lvl);
}

void QSenderSubscriptions::setTopics(const QStringList& topics) {

    // Remove old topics
    topics_.clear();

    // Add new topics
    for(const auto& topic : topics) {
        auto* item = new QStandardItem(topic);
        topics_.appendRow(item);
    }
}
