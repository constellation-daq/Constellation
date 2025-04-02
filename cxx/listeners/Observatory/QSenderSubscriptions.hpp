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

#include <QComboBox>
#include <QItemSelection>
#include <QStandardItemModel>
#include <QStringList>
#include <QWidget>

#include "constellation/core/log/Level.hpp"
#include "constellation/gui/QLogLevelComboBox.hpp"

// Expose Qt class auto-generated from the user interface XML:
namespace Ui { // NOLINT(readability-identifier-naming)
    class QSenderSubscriptions;
} // namespace Ui

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

class QSenderSubscriptions : public QWidget {
    Q_OBJECT

public:
    QSenderSubscriptions(QWidget* parent,
                         QString name,
                         std::function<void(const std::string&, const std::string&, constellation::log::Level)> sub_callback,
                         std::function<void(const std::string&, const std::string&)> unsub_callback);

    void setTopics(const QStringList& topics);

    void updateListItemSize();

signals:
    void sizeChanged();

private slots:
    /**
     * @brief Private slot for changes of the sender log level setting
     *
     * @param index New index of the log level
     */
    void on_senderLevel_currentIndexChanged(int index);

private:
    QString name_;
    std::shared_ptr<Ui::QSenderSubscriptions> ui_;
    std::function<void(const std::string&, const std::string&, constellation::log::Level)> sub_callback_;
    std::function<void(const std::string&, const std::string&)> unsub_callback_;

    // Topics
    ComboBoxItemDelegate delegate_;
    QStandardItemModel topics_;
};
