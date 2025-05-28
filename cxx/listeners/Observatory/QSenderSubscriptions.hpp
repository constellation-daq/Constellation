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
#include <string>

#include <QApplication>
#include <QComboBox>
#include <QGridLayout>
#include <QItemSelection>
#include <QPropertyAnimation>
#include <QStandardItemModel>
#include <QStringList>
#include <QTableView>
#include <QWidget>

#include "constellation/core/log/Level.hpp"
#include "constellation/gui/QCollapseButton.hpp"
#include "constellation/gui/QLogLevelComboBox.hpp"

/**
 * @class ComboBoxItemDelegate
 * @brief Delegate to paint ComboBoxes as editors in a QTableView
 */
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

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// @endcond
};

/**
 * @class QSenderSubscriptions
 * @brief Class providing a user interface to topical subscriptions for a single sending host
 */
class QSenderSubscriptions : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     *
     * @param name Canonical name of the host
     * @param sub_callback Callback for topical subscription from this host
     * @param unsub_callback Callback for topical unsubscriptions from this host
     * @param topics Optional list of initially available topics
     * @param parent Pointer to parent widget
     */
    explicit QSenderSubscriptions(
        QString name,
        std::function<void(const std::string&, const std::string&, constellation::log::Level)> sub_callback,
        std::function<void(const std::string&, const std::string&)> unsub_callback,
        const QStringList& topics = {},
        QWidget* parent = nullptr);

    /**
     * @brief Get canonical name of the host
     * @return Name of the hist this widget is managing subscriptions for
     */
    QString getName() const { return name_; }

    /**
     * @brief Collapse the topic list
     * @details This helper allows to collapse the topic list of this widget programmatically, used by the subscription list
     */
    void collapse();

    /**
     * @brief Set the available topics
     * @details This clears previously available topics and sets the list with these.
     *
     * @param topics List of available topics
     */
    void setTopics(const QStringList& topics);

signals:
    /**
     * @brief Signal emitted when the topic list was expanded or collapsed
     *
     * @param item Pointer to this subscription item
     * @param expanded Boolean indicating whether it was collapsed or expanded
     */
    void expanded(QSenderSubscriptions* item, bool expanded); // Signal to notify expansion

private:
    /**
     * @brief Helper to re-calculate the widget height and set the animation when expanding or collapsing the topic list
     *
     * @param expand Boolean selecting whether to expand or collapse the topic list
     */
    void update_height(bool expand);

private:
    // Name of the host
    QString name_;

    // Callbacks for topic subscriptions
    std::function<void(const std::string&, const std::string&, constellation::log::Level)> sub_callback_;
    std::function<void(const std::string&, const std::string&)> unsub_callback_;

    // Delegate to draw comboboxes in the topics list
    ComboBoxItemDelegate delegate_;

    // Log level combobox for the sender-wide topic
    constellation::gui::QLogLevelComboBox* sender_level_;

    // UI elements
    constellation::gui::QCollapseButton* expand_button_;
    QTableView* topics_view_;
    QStandardItemModel* topics_;
    QWidget* container_;
    QGridLayout* main_layout_;
    QPropertyAnimation* animation_;
};
