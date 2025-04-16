/**
 * @file
 * @brief Log Level ComboBox Implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QLogLevelComboBox.hpp"

#include <utility>

#include <QPaintEvent>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/gui/qt_utils.hpp"

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

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(100, 25);

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

void QLogLevelComboBox::addNeutralElement(std::string neutral) {
    neutral_ = std::move(neutral);
    fill_items();
}
