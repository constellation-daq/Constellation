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
#include <iostream>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/utils/enum.hpp"

#include "ui_QSenderSubscriptions.h"

using namespace constellation;
using namespace constellation::log;
using namespace constellation::utils;

ComboBoxItemDelegate::ComboBoxItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QWidget* ComboBoxItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const {
    // Create the combobox and populate it
    QComboBox* cb = new QComboBox(parent);
    cb->addItem("(global)");
    for(int level_it = std::to_underlying(Level::CRITICAL); level_it >= std::to_underlying(Level::TRACE); --level_it) {
        cb->addItem(QString::fromStdString(enum_name(Level(level_it))));
    }
    return cb;
}

void ComboBoxItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    QComboBox* cb = qobject_cast<QComboBox*>(editor);
    Q_ASSERT(cb);
    // get the index of the text in the combobox that matches the current value of the item
    const QString currentText = index.data(Qt::EditRole).toString();
    const int cbIndex = cb->findText(currentText);
    // if it is valid, adjust the combobox
    if(cbIndex >= 0)
        cb->setCurrentIndex(cbIndex);
}

void ComboBoxItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    QComboBox* cb = qobject_cast<QComboBox*>(editor);
    Q_ASSERT(cb);
    model->setData(index, cb->currentText(), Qt::EditRole);
}

QSenderSubscriptions::QSenderSubscriptions(QWidget* parent,
                                           const QString& name,
                                           std::function<void(const std::string&, const std::string&, Level)> sub_callback,
                                           std::function<void(const std::string&, const std::string&)> unsub_callback)
    : QWidget(parent), name_(name), ui_(new Ui::QSenderSubscriptions), sub_callback_(std::move(sub_callback)),
      unsub_callback_(std::move(unsub_callback)), delegate_(this) {
    ui_->setupUi(this);
    ui_->senderName->setText(name_);
    ui_->topicsView->setModel(&topics_);
    ui_->topicsView->setItemDelegateForColumn(1, &delegate_);

    // Connect selection change from topics list:
    connect(ui_->topicsView->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            [&](const QItemSelection& selected, const QItemSelection& deselected) {
                for(const auto& idx : selected.indexes()) {
                    const auto topic = topics_.itemFromIndex(idx)->text().toStdString();
                    sub_callback_(name_.toStdString(), topic, Level::TRACE);
                }
                for(const auto& idx : deselected.indexes()) {
                    const auto topic = topics_.itemFromIndex(idx)->text().toStdString();
                    unsub_callback_(name_.toStdString(), topic);
                }
            });
    connect(&topics_, &QStandardItemModel::itemChanged, this, [&](QStandardItem* item) {
        const auto topic = topics_.item(item->index().row())->text().toStdString();
        const auto level = enum_cast<Level>(item->text().toStdString());
        if(level.has_value()) {
            sub_callback_(name_.toStdString(), topic, level.value());
        } else {
            unsub_callback_(name_.toStdString(), topic);
        }
    });
}

void QSenderSubscriptions::on_senderLevel_currentIndexChanged(int index) {
    const auto lvl = Level(index);
    sub_callback_(name_.toStdString(), "", lvl);
}

void QSenderSubscriptions::setTopics(const QStringList& topics) {

    // Remove old topics
    topics_.clear();
    topics_.setColumnCount(2);

    // Add new topics
    for(const auto& topic : topics) {
        QList<QStandardItem*> row;
        auto* item = new QStandardItem(topic);
        row.append(item);
        auto* item2 = new QStandardItem();
        row.append(item2);
        topics_.appendRow(row);
        ui_->topicsView->openPersistentEditor(item2->index());
    }
}
