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

#include <QPaintEvent>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/gui/qt_utils.hpp"

#include "ui_QSenderSubscriptions.h"

using namespace constellation;
using namespace constellation::gui;
using namespace constellation::log;
using namespace constellation::utils;

void QLogLevelDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    auto options = option;

    // Get log level color
    const auto level_str = index.data().toString().toStdString();
    const auto level = enum_cast<Level>(level_str).value_or(INFO);

    const auto color = get_log_level_color(level);
    if(level > Level::INFO) {
        // High levels get background coloring
        painter->fillRect(options.rect, QBrush(color));
    } else {
        // Others just text color adjustment
        options.palette.setColor(QPalette::Text, color);
    }

    QStyledItemDelegate::paint(painter, options, index);
}

void QLogLevelComboBox::paintEvent(QPaintEvent* event) {
    QComboBox::paintEvent(event);

    const auto item = this->itemData(this->currentIndex(), Qt::DisplayRole);
    if(!item.isNull()) {
        QPainter painter(this);
        QStyleOptionViewItem option;
        option.initFrom(this);

        // Get log level color
        const auto level_str = item.toString().toStdString();
        const auto level = enum_cast<Level>(level_str).value_or(INFO);

        const auto color = get_log_level_color(level);
        if(level > Level::INFO) {
            // High levels get background coloring
            painter.fillRect(event->rect(), QBrush(color));
        } else {
            // Others just text color adjustment
            option.palette.setColor(QPalette::Text, color);
        }
    }
}

QLogLevelComboBox::QLogLevelComboBox(QWidget* parent) : QComboBox(parent) {
    fill_items();

    // Set item delegate:
    setItemDelegate(&delegate_);
}

void QLogLevelComboBox::setCurrentLevel(constellation::log::Level level) {
    auto idx = findText(QString::fromStdString(enum_name(level)), Qt::MatchFixedString);
    if(idx > -1) {
        setCurrentIndex(idx);
    }
}

void QLogLevelComboBox::fill_items() {
    clear();

    if(!neutral_.empty()) {
        addItem(QString::fromStdString(neutral_));
    }

    if(descending_) {
        for(int level_it = std::to_underlying(Level::TRACE); level_it <= std::to_underlying(Level::CRITICAL); ++level_it) {
            addItem(QString::fromStdString(enum_name(Level(level_it))));
        }
    } else {
        for(int level_it = std::to_underlying(Level::CRITICAL); level_it >= std::to_underlying(Level::TRACE); --level_it) {
            addItem(QString::fromStdString(enum_name(Level(level_it))));
        }
    }
}

void QLogLevelComboBox::setDescending(bool descending) {
    descending_ = descending;
    fill_items();
}

void QLogLevelComboBox::addNeutralElement(const std::string& neutral) {
    neutral_ = neutral;
    fill_items();
}

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
}

void QSenderSubscriptions::on_senderLevel_currentIndexChanged(int index) {
    const auto level = enum_cast<Level>(ui_->senderLevel->itemText(index).toStdString());
    if(level.has_value()) {
        sub_callback_(name_.toStdString(), "", level.value());
    } else {
        unsub_callback_(name_.toStdString(), "");
    }
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
