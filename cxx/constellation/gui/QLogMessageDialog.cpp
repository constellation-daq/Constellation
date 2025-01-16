/**
 * @file
 * @brief Log message dialog
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QLogMessageDialog.hpp"

#include <QDialog>

#include "constellation/gui/QLogMessage.hpp"
#include "constellation/gui/qt_utils.hpp"

#include "ui_QLogMessageDialog.h"

using namespace constellation::gui;

QLogMessageDialog::QLogMessageDialog(QWidget* parent, const QLogMessage& msg)
    : QDialog(parent), ui_(new Ui::QLogMessageDialog) {
    ui_->setupUi(this);

    ui_->senderName->setText("<font color='gray'><b>" + msg[1].toString() + "</b></font>");

    auto palette = ui_->logLevel->palette();
    palette.setColor(QPalette::WindowText, get_log_level_color(msg.getLogLevel()));
    ui_->logLevel->setPalette(palette);
    ui_->logLevel->setText("<b>" + msg[2].toString() + "</b>");

    ui_->messageTable->setRowCount(QLogMessage::countExtendedColumns());
    ui_->messageTable->setColumnCount(2);
    ui_->messageTable->setHorizontalHeaderLabels({"Key", "Value"});
    ui_->messageTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    for(int i = 0; i < QLogMessage::countExtendedColumns(); i++) {
        // QTableWidget takes ownership of assigned QTableWidgetItems
        // NOLINTBEGIN(cppcoreguidelines-owning-memory)
        ui_->messageTable->setItem(i, 0, new QTableWidgetItem(QLogMessage::columnName(i)));
        ui_->messageTable->setItem(i, 1, new QTableWidgetItem(msg[i].toString()));
        // NOLINTEND(cppcoreguidelines-owning-memory)
    }

    show();
}
