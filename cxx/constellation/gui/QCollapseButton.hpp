/**
 * @file
 * @brief Collapse button helper
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <QString>
#include <QToolButton>
#include <QWidget>

#include "constellation/build.hpp"

namespace constellation::gui {

    /**
     * @class QCollapseButton
     * @brief Helper class to draw a small button with arrow and no border to collapse and expand UI elements
     */
    class CNSTLN_API QCollapseButton : public QToolButton {
    public:
        QCollapseButton(QWidget* parent = nullptr);
        QCollapseButton(const QString& text, QWidget* parent = nullptr);
        void setText(const QString& text) { QToolButton::setText(" " + text); }
    };

} // namespace constellation::gui
