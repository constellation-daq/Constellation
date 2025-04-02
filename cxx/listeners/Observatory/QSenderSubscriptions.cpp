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
#include <utility>

#include <QListWidget>
#include <QPaintEvent>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/gui/qt_utils.hpp"

#include "ui_QSenderSubscriptions.h"

using namespace constellation;
using namespace constellation::gui;
using namespace constellation::log;
using namespace constellation::utils;

ComboBoxItemDelegate::ComboBoxItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QWidget* ComboBoxItemDelegate::createEditor(QWidget* parent,
                                            const QStyleOptionViewItem& /*option*/,
                                            const QModelIndex& /*index*/) const {
    // Create the combobox and populate it
    // Ownership is transferred to the caller
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    auto* box = new QLogLevelComboBox(parent);
    box->setDescending(false);
    box->addNeutralElement("- global -");
    return box;
}

void ComboBoxItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    auto* box = dynamic_cast<QComboBox*>(editor);

    // get the index of the text in the combobox that matches the current value of the item
    const QString currentText = index.data(Qt::EditRole).toString();
    const int idx = box->findText(currentText);
    // if it is valid, adjust the combobox
    if(idx >= 0) {
        box->setCurrentIndex(idx);
    }
}

void ComboBoxItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    auto* box = dynamic_cast<QComboBox*>(editor);
    model->setData(index, box->currentText(), Qt::EditRole);
}

QSenderSubscriptions::QSenderSubscriptions(QWidget* parent,
                                           QString name,
                                           std::function<void(const std::string&, const std::string&, Level)> sub_callback,
                                           std::function<void(const std::string&, const std::string&)> unsub_callback)
    : QWidget(parent), name_(std::move(name)), ui_(new Ui::QSenderSubscriptions), sub_callback_(std::move(sub_callback)),
      unsub_callback_(std::move(unsub_callback)), delegate_(this) {
    ui_->setupUi(this);
    ui_->senderName->setText(name_);
    ui_->topicsView->setModel(&topics_);
    ui_->topicsView->setItemDelegateForColumn(1, &delegate_);

    ui_->senderLevel->setDescending(false);
    ui_->senderLevel->addNeutralElement("- global -");

    ui_->collapseButton->setText("Log Topics");
    ui_->collapseButton->setContent(ui_->topicsView);

    // Connect item change to subscription:
    connect(&topics_, &QStandardItemModel::itemChanged, this, [&](QStandardItem* item) {
        const auto topic = topics_.item(item->index().row())->text().toStdString();
        const auto level = enum_cast<Level>(item->text().toStdString());
        if(level.has_value()) {
            sub_callback_(name_.toStdString(), topic, level.value());
        } else {
            unsub_callback_(name_.toStdString(), topic);
        }
    });

    connect(ui_->collapseButton, &QToolButton::toggled, this, &QSenderSubscriptions::updateListItemSize);
}

void QSenderSubscriptions::on_senderLevel_currentIndexChanged(int index) {
    const auto level = enum_cast<Level>(ui_->senderLevel->itemText(index).toStdString());
    if(level.has_value()) {
        sub_callback_(name_.toStdString(), "", level.value());
    } else {
        unsub_callback_(name_.toStdString(), "");
    }
}

void QSenderSubscriptions::updateListItemSize() {
    adjustSize(); // Ensure the widget computes its new height

    if(auto* listWidget = dynamic_cast<QListWidget*>(parentWidget())) {
        if(auto* item = listWidget->itemAt(pos())) {
            item->setSizeHint(sizeHint());
            listWidget->updateGeometry();
        }
    }
    emit sizeChanged();
}

void QSenderSubscriptions::setTopics(const QStringList& topics) {

    // Remove old topics
    topics_.clear();
    topics_.setColumnCount(2);

    // Add new topics
    for(const auto& topic : topics) {
        // Underlying QStandardItemModel takes ownership of QStandardItem instances
        // NOLINTBEGIN(cppcoreguidelines-owning-memory)
        QList<QStandardItem*> row;
        row.append(new QStandardItem(topic));
        auto* item2 = new QStandardItem();
        row.append(item2);
        topics_.appendRow(row);
        ui_->topicsView->openPersistentEditor(item2->index());
        // NOLINTEND(cppcoreguidelines-owning-memory)
    }
}
