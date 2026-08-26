/**
 * @file
 * @brief Collapse button helper
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QCollapseButton.hpp"

#include <algorithm>

#include <QApplication>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QString>
#include <QToolButton>
#include <QWidget>

using namespace constellation::gui;

namespace {
    // Approximate width of arrow, icon, padding and leading space of the text
    constexpr int icon_width = 34;
} // namespace

QCollapseButton::QCollapseButton(QWidget* parent) : QToolButton(parent) {
    setCheckable(true);
    setStyleSheet("QToolButton { border-style: outset; border-width: 0px; font-weight: normal; }");
    setFont(QApplication::font());
    setArrowType(Qt::ArrowType::RightArrow);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);

    connect(this, &QToolButton::toggled, [&](bool checked) {
        setArrowType(checked ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
    });
}

QCollapseButton::QCollapseButton(const QString& text, QWidget* parent) : QCollapseButton(parent) {
    setText(text);
}

QSize QCollapseButton::minimumSizeHint() const {
    const QFontMetrics fm(font());
    const int text_floor = fm.horizontalAdvance(QStringLiteral("..."));
    return {text_floor + icon_width, QToolButton::sizeHint().height()};
}

void QCollapseButton::resizeEvent(QResizeEvent* event) {
    QToolButton::resizeEvent(event);
    update_elided_text();
}

void QCollapseButton::update_elided_text() {
    const QFontMetrics fm(font());
    const int available = std::max(width() - icon_width, 0);
    QToolButton::setText(" " + fm.elidedText(label_text_, Qt::ElideRight, available));
}
