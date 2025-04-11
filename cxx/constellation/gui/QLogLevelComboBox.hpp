/**
 * @file
 * @brief Log Level ComboBox with colored entries
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>

#include <QComboBox>
#include <QPainter>
#include <QStyledItemDelegate>

#include "constellation/build.hpp"
#include "constellation/core/log/Level.hpp"

namespace constellation::gui {

    class CNSTLN_API QLogLevelDelegate : public QStyledItemDelegate {
        Q_OBJECT

    public:
        QLogLevelDelegate() = default;

    private:
        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    };

    class CNSTLN_API QLogLevelComboBox : public QComboBox {
        Q_OBJECT

    public:
        QLogLevelComboBox(QWidget* parent = nullptr);
        virtual ~QLogLevelComboBox() = default;

        // No copy constructor/assignment/move constructor/assignment
        /// @cond doxygen_suppress
        QLogLevelComboBox(const QLogLevelComboBox& other) = delete;
        QLogLevelComboBox& operator=(const QLogLevelComboBox& other) = delete;
        QLogLevelComboBox(QLogLevelComboBox&& other) noexcept = delete;
        QLogLevelComboBox& operator=(QLogLevelComboBox&& other) = delete;
        /// @endcond

        void setDescending(bool descending);
        void addNeutralElement(std::string neutral);

        void setCurrentLevel(constellation::log::Level level);

    private:
        void paintEvent(QPaintEvent* event) override;

        void fill_items();

        bool descending_ {false};
        std::string neutral_;
        QLogLevelDelegate delegate_;
    };
} // namespace constellation::gui
