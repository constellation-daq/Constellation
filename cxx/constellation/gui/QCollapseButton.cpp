/**
 * @file
 * @brief Collapse button helper
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QCollapseButton.hpp"

#include <QApplication>
#include <QString>
#include <QToolButton>
#include <QWidget>

using namespace constellation::gui;

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
    QToolButton::setText(" " + text);
}
