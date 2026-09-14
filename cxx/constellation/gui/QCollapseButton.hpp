/**
 * @file
 * @brief Collapse button helper
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <QSize>
#include <QString>
#include <QToolButton>
#include <QWidget>

#include "constellation/build.hpp"

class QResizeEvent;

namespace constellation::gui {

    /**
     * @class QCollapseButton
     * @brief Helper class to draw a small button with arrow and no border to collapse and expand UI elements
     */
    class CNSTLN_API QCollapseButton : public QToolButton {
    public:
        QCollapseButton(QWidget* parent = nullptr);
        QCollapseButton(const QString& text, QWidget* parent = nullptr);

        /**
         * @brief Set the label text
         * @details The label is elided with "..." whenever the button is not wide enough to display the full label
         *
         * @param text Full label text
         */
        void setText(const QString& text) { // NOLINT(bugprone-derived-method-shadowing-base-method)
            label_text_ = text;
            update_elided_text();
        }

        /**
         * @brief Text-independent minimum size hint
         * @details Instead of setting the minimum size based on the text, this button elides its label
         *
         * @return Minimum size hint
         */
        QSize minimumSizeHint() const override;

    protected:
        /**
         * @brief Resize event handler, used to elide label to fit the new width
         *
         * @param event Resize event
         */
        void resizeEvent(QResizeEvent* event) override;

    private:
        /**
         * @brief Recompute and apply the elided version of full label text
         */
        void update_elided_text();

    private:
        // Full label text
        QString label_text_;
    };

} // namespace constellation::gui
