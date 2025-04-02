/**
 * @file
 * @brief Sender Subscription Widget
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <functional>
#include <memory>
#include <string>

#include <QApplication>
#include <QComboBox>
#include <QGridLayout>
#include <QItemSelection>
#include <QLabel>
#include <QPropertyAnimation>
#include <QStandardItemModel>
#include <QStringList>
#include <QTableView>
#include <QToolButton>
#include <QWidget>

#include "constellation/core/log/Level.hpp"
#include "constellation/gui/QLogLevelComboBox.hpp"

class ComboBoxItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    ComboBoxItemDelegate(QObject* parent = nullptr);
    virtual ~ComboBoxItemDelegate() = default;

    // No copy constructor/assignment/move constructor/assignment
    /// @cond doxygen_suppress
    ComboBoxItemDelegate(const ComboBoxItemDelegate& other) = delete;
    ComboBoxItemDelegate& operator=(const ComboBoxItemDelegate& other) = delete;
    ComboBoxItemDelegate(ComboBoxItemDelegate&& other) noexcept = delete;
    ComboBoxItemDelegate& operator=(ComboBoxItemDelegate&& other) = delete;
    /// @endcond

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
};

class QCollapseButton : public QToolButton {
public:
    QCollapseButton(QWidget* parent = nullptr);
    void setText(const QString& text) { QToolButton::setText(" " + text); }
};

class QSenderSubscriptions : public QWidget {
    Q_OBJECT

public:
    explicit QSenderSubscriptions(
        const QString& name,
        std::function<void(const std::string&, const std::string&, constellation::log::Level)> sub_callback,
        std::function<void(const std::string&, const std::string&)> unsub_callback,
        const QStringList& topics = {},
        QWidget* parent = nullptr);

    QString getName() const { return name_; }
    void toggleExpand();
    void setTopics(const QStringList& topics);

signals:
    void expanded(QSenderSubscriptions* item); // Signal to notify expansion

private:
    QString name_;

    std::function<void(const std::string&, const std::string&, constellation::log::Level)> sub_callback_;
    std::function<void(const std::string&, const std::string&)> unsub_callback_;

    ComboBoxItemDelegate delegate_;

    QLabel* label_;
    QCollapseButton* expand_button_;
    constellation::gui::QLogLevelComboBox* sender_level_;
    QTableView* topics_view_;
    QStandardItemModel* topics_;
    QWidget* container_;
    QGridLayout* main_layout_;
    QPropertyAnimation* animation_;
    bool m_isExpanded;

    void updateExpansionHeight();
};
