/**
 * @file
 * @brief Log Filter
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>

#include <QAbstractAnimation>
#include <QApplication>
#include <QPropertyAnimation>
#include <QToolButton>

class QCollapseButton : public QToolButton {
public:
    QCollapseButton(QWidget* parent) : QToolButton(parent) {
        setCheckable(true);
        setStyleSheet("QToolButton { border-style: outset; border-width: 0px; font-size: 12px; font-weight: normal; }");

        setIconSize(QSize(8, 8));
        setFont(QApplication::font());
        setArrowType(Qt::ArrowType::RightArrow);
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

        connect(this, &QToolButton::toggled, [&](bool checked) {
            setArrowType(checked ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
            content_ != nullptr&& checked ? showContent() : hideContent();
        });
    }

    void setText(const QString& text) { QToolButton::setText(" " + text); }

    void setContent(QWidget* content) {
        content_ = content;
        animation_ = std::make_shared<QPropertyAnimation>(content_, "maximumHeight");
        animation_->setStartValue(0);
        animation_->setEasingCurve(QEasingCurve::InOutQuad);
        animation_->setDuration(300);
        animation_->setEndValue(content->geometry().height() + 10);
        if(!isChecked()) {
            content->setMaximumHeight(0);
        }
    }

    void hideContent() {
        animation_->setDirection(QAbstractAnimation::Backward);
        animation_->start();
    }

    void showContent() {
        animation_->setDirection(QAbstractAnimation::Forward);
        animation_->start();
    }

private:
    QWidget* content_ {nullptr};
    std::shared_ptr<QPropertyAnimation> animation_;
};
