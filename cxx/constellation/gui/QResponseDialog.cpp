/**
 * @file
 * @brief Response dialog
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QResponseDialog.hpp"

#include <string>
#include <string_view>

#include <QDialog>
#include <QString>

#include "constellation/core/config/Dictionary.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/gui/qt_utils.hpp"

#include "ui_QResponseDialog.h"

using namespace constellation::config;
using namespace constellation::gui;
using namespace constellation::message;

QResponseDialog::QResponseDialog(QWidget* parent, const CSCP1Message& message)
    : QDialog(parent), ui_(new Ui::QResponseDialog) {

    ui_->setupUi(this);
    setSizeGripEnabled(true);
    setWindowTitle("Satellite Response");

    // Set satellite name:
    ui_->satelliteName->setText("<font color='gray'><b>" +
                                QString::fromStdString(std::string(message.getHeader().getSender())) + "</b></font>");
    ui_->satelliteResponse->setText("<font color='gray'><b>" + get_styled_response(message.getVerb().first) + "</b></font>");
    ui_->responseVerb->setText(QString::fromStdString(std::string(message.getVerb().second)) + ":");

    ui_->responseTable->setVisible(true);
    ui_->responseText->setVisible(false);

    const auto& payload = message.getPayload();
    // Decode payload:
    if(!payload.empty()) {
        try {
            const auto dict = Dictionary::disassemble(payload);
            show_as_dictionary(dict);
        } catch(...) {
            try {
                const auto list = List::disassemble(payload);
                show_as_list(list);
            } catch(...) {
                try {
                    const auto val = Value::disassemble(payload).str();
                    show_as_string(val);
                } catch(...) {
                    show_as_string(payload.to_string_view());
                }
            }
        }
    }
}

void QResponseDialog::show_as_dictionary(const Dictionary& dict) {
    ui_->responseTable->setRowCount(dict.size());
    ui_->responseTable->setColumnCount(2);
    ui_->responseTable->setHorizontalHeaderLabels({"Key", "Value"});
    ui_->responseTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto it = dict.begin();
    for(std::size_t idx = 0; idx < dict.size(); idx++) {
        ui_->responseTable->setItem(idx, 0, new QTableWidgetItem(QString::fromStdString(it->first)));
        ui_->responseTable->setItem(idx, 1, new QTableWidgetItem(QString::fromStdString(it->second.str())));
        std::advance(it, 1);
    }
}

void QResponseDialog::show_as_list(const List& list) {
    ui_->responseTable->setRowCount(list.size());
    ui_->responseTable->setColumnCount(1);
    ui_->responseTable->setHorizontalHeaderLabels({"Value"});
    ui_->responseTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto it = list.begin();
    for(std::size_t idx = 0; idx < list.size(); idx++) {
        ui_->responseTable->setItem(idx, 0, new QTableWidgetItem(QString::fromStdString(it->str())));
        std::advance(it, 1);
    }
}

void QResponseDialog::show_as_string(std::string_view str) {
    ui_->responseTable->setVisible(false);
    ui_->responseText->setVisible(true);
    ui_->responseText->setReadOnly(true);
    ui_->responseText->setText(QString::fromStdString(std::string(str)));
}
