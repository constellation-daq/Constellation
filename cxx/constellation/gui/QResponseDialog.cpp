/**
 * @file
 * @brief Response dialog
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QResponseDialog.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>

#include <QDialog>
#include <QString>
#include <QTreeWidgetItem>

#include "constellation/core/config/value_types.hpp"
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
    ui_->satelliteResponse->setText("<b>" + get_styled_response(message.getVerb().first) + "</b>");
    ui_->responseVerb->setText(QString::fromStdString(std::string(message.getVerb().second)));

    ui_->responseTree->setVisible(true);
    ui_->responseText->setVisible(false);

    const auto& payload = message.getPayload();
    // Decode payload:
    if(!payload.empty()) {
        try {
            const auto dict = Dictionary::disassemble(payload);
            show_as_dictionary(dict);
        } catch(...) {
            try {
                const auto list = CompositeList::disassemble(payload);
                show_as_list(list);
            } catch(...) {
                try {
                    const auto val = Composite::disassemble(payload).to_string();
                    show_as_string(val);
                } catch(...) {
                    show_as_string(payload.to_string_view());
                }
            }
        }
    }
}

void QResponseDialog::setup_tree_widget(const QString& key_header, const QString& value_header) {
    ui_->responseTree->setColumnCount(2);
    ui_->responseTree->setHeaderLabels({key_header, value_header});
    ui_->responseTree->setAlternatingRowColors(true);
    ui_->responseTree->header()->setStretchLastSection(true);
    ui_->responseTree->header()->setDefaultSectionSize(200);

    // Better hierarchy visibility
    ui_->responseTree->setIndentation(30);
    ui_->responseTree->setRootIsDecorated(true);
    ui_->responseTree->setAnimated(true);
}

void QResponseDialog::show_as_dictionary(const Dictionary& dict) {
    setup_tree_widget("Key", "Value");
    populate_tree_item_dict(nullptr, dict);
    ui_->responseTree->expandAll();
    ui_->responseTree->resizeColumnToContents(0);
}

void QResponseDialog::show_as_list(const CompositeList& list) {
    setup_tree_widget("Index", "Value");
    populate_tree_item_list(nullptr, list);
    ui_->responseTree->expandAll();
    ui_->responseTree->resizeColumnToContents(0);
}

void QResponseDialog::show_as_string(std::string_view str) {
    ui_->responseTree->setVisible(false);
    ui_->responseText->setVisible(true);
    ui_->responseText->setReadOnly(true);
    ui_->responseText->setText(QString::fromStdString(std::string(str)));
}

// NOLINTBEGIN(misc-no-recursion)
void QResponseDialog::populate_tree_item(QTreeWidgetItem* parent, const std::string& key, const Composite& value) {

    // Set child item if available, nullptr parent indicates root node
    auto* item = (parent == nullptr ? new QTreeWidgetItem(ui_->responseTree) : new QTreeWidgetItem(parent));
    item->setText(0, QString::fromStdString(key));

    // Check Composite type
    if(std::holds_alternative<Dictionary>(value)) {
        const auto& dict = std::get<Dictionary>(value);
        item->setText(1, QString::fromStdString("{...} (" + std::to_string(dict.size()) + " items)"));
        populate_tree_item_dict(item, dict);
    } else if(std::holds_alternative<Array>(value)) {
        const auto& arr = std::get<Array>(value);
        item->setText(1, QString::fromStdString(arr.to_string()));
    } else if(std::holds_alternative<Scalar>(value)) {
        const auto& scalar = std::get<Scalar>(value);
        item->setText(1, QString::fromStdString(scalar.to_string()));
    }
}

void QResponseDialog::populate_tree_item_dict(QTreeWidgetItem* parent, const Dictionary& dict) {
    for(const auto& [key, value] : dict) {
        populate_tree_item(parent, key, value);
    }
}

void QResponseDialog::populate_tree_item_list(QTreeWidgetItem* parent, const CompositeList& list) {
    for(std::size_t idx = 0; idx < list.size(); ++idx) {
        const auto& item_value = list[idx];

        auto* item = (parent == nullptr ? new QTreeWidgetItem(ui_->responseTree) : new QTreeWidgetItem(parent));
        item->setText(0, QString::fromStdString("[" + std::to_string(idx) + "]"));

        // Check Composite type
        if(std::holds_alternative<Dictionary>(item_value)) {
            const auto& dict = std::get<Dictionary>(item_value);
            item->setText(1, QString::fromStdString("{...} (" + std::to_string(dict.size()) + " items)"));
            populate_tree_item_dict(item, dict);
        } else if(std::holds_alternative<Array>(item_value)) {
            const auto& arr = std::get<Array>(item_value);
            item->setText(1, QString::fromStdString(arr.to_string()));
        } else if(std::holds_alternative<Scalar>(item_value)) {
            const auto& scalar = std::get<Scalar>(item_value);
            item->setText(1, QString::fromStdString(scalar.to_string()));
        }
    }
}
// NOLINTEND(misc-no-recursion)
