/**
 * @file
 * @brief Connection detail dialog
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QConnectionDialog.hpp"

#include <iterator>
#include <string>
#include <utility>

#include <QDialog>
#include <QPainter>
#include <QString>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTextDocument>

#include "constellation/core/config/Dictionary.hpp"

#include "ui_QConnectionDialog.h"

using namespace constellation::config;
using namespace constellation::gui;
using namespace constellation::message;

void ConnectionDialogItemDelegate::paint(QPainter* painter,
                                         const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const {
    auto options = option;
    initStyleOption(&options, index);
    painter->save();

    QTextDocument doc;
    doc.setHtml(options.text);

    options.text = "";
    options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter);

    painter->translate(options.rect.left(), options.rect.top());
    const QRect clip(0, 0, options.rect.width(), options.rect.height());
    doc.drawContents(painter, clip);

    painter->restore();
}

QConnectionDialog::QConnectionDialog(QWidget* parent,
                                     const std::string& name,
                                     const QMap<QString, QVariant>& details,
                                     const config::Dictionary& commands)
    : QDialog(parent), ui_(new Ui::QConnectionDialog) {

    ui_->setupUi(this);
    setSizeGripEnabled(true);
    setWindowTitle("Satellite Connection Details");

    // Set header information:
    ui_->satelliteName->setText("<font color='gray'><b>" + QString::fromStdString(name) + "</b></font>");
    ui_->satelliteState->setText(details.value("State").toString());

    // Set connection details:
    ui_->connectionTable->setRowCount(static_cast<int>(details.size()));
    ui_->connectionTable->setColumnCount(2);
    ui_->connectionTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui_->connectionTable->setItemDelegate(&item_delegate_);

    auto it = details.cbegin();
    for(int idx = 0; idx < static_cast<int>(details.size()); idx++) {
        // QTableWidget takes ownership of assigned QTableWidgetItems
        // NOLINTBEGIN(cppcoreguidelines-owning-memory)
        ui_->connectionTable->setItem(idx, 0, new QTableWidgetItem(it.key()));
        ui_->connectionTable->setItem(idx, 1, new QTableWidgetItem(it.value().toString()));
        // NOLINTEND(cppcoreguidelines-owning-memory)
        std::advance(it, 1);
    }

    // Set commands:
    show_commands(commands);

    show();
}

void QConnectionDialog::show_commands(const Dictionary& dict) {
    ui_->commandTable->setRowCount(static_cast<int>(dict.size()));
    ui_->commandTable->setColumnCount(2);
    ui_->commandTable->setHorizontalHeaderLabels({"Command", "Description"});
    ui_->commandTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto it = dict.begin();
    for(int idx = 0; std::cmp_less(idx, dict.size()); idx++) {
        // QTableWidget takes ownership of assigned QTableWidgetItems
        // NOLINTBEGIN(cppcoreguidelines-owning-memory)
        ui_->commandTable->setItem(idx, 0, new QTableWidgetItem(QString::fromStdString(it->first)));
        ui_->commandTable->setItem(idx, 1, new QTableWidgetItem(QString::fromStdString(it->second.str())));
        // NOLINTEND(cppcoreguidelines-owning-memory)
        std::advance(it, 1);
    }
}
