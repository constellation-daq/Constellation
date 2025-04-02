/**
 * @file
 * @brief Sender Subscription Implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QSenderSubscriptions.hpp"

#include <functional>
#include <utility>

#include <QHeaderView>
#include <QListWidget>
#include <QPaintEvent>
#include <QToolButton>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/gui/qt_utils.hpp"

using namespace constellation;
using namespace constellation::gui;
using namespace constellation::log;
using namespace constellation::utils;

ComboBoxItemDelegate::ComboBoxItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QWidget* ComboBoxItemDelegate::createEditor(QWidget* parent,
                                            const QStyleOptionViewItem& /*option*/,
                                            const QModelIndex& /*index*/) const {
    // Create the combobox and populate it
    // Ownership is transferred to the caller
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    auto* box = new QLogLevelComboBox(parent);
    box->setDescending(false);
    box->addNeutralElement("- global -");
    return box;
}

void ComboBoxItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    auto* box = dynamic_cast<QComboBox*>(editor);

    // get the index of the text in the combobox that matches the current value of the item
    const QString currentText = index.data(Qt::EditRole).toString();
    const int idx = box->findText(currentText);
    // if it is valid, adjust the combobox
    if(idx >= 0) {
        box->setCurrentIndex(idx);
    }
}

void ComboBoxItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    auto* box = dynamic_cast<QComboBox*>(editor);
    model->setData(index, box->currentText(), Qt::EditRole);
}

QCollapseButton::QCollapseButton(QWidget* parent) : QToolButton(parent) {
    setCheckable(true);
    setStyleSheet("QToolButton { border-style: outset; border-width: 0px; font-size: 12px; font-weight: normal; }");

    setIconSize(QSize(8, 8));
    setFont(QApplication::font());
    setArrowType(Qt::ArrowType::RightArrow);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    connect(this, &QToolButton::toggled, [&](bool checked) {
        setArrowType(checked ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
    });
}

QSenderSubscriptions::QSenderSubscriptions(const QString& name,
                                           std::function<void(const std::string&, const std::string&, Level)> sub_callback,
                                           std::function<void(const std::string&, const std::string&)> unsub_callback,
                                           const QStringList& topics,
                                           QWidget* parent)
    : QWidget(parent), name_(name), sub_callback_(std::move(sub_callback)), unsub_callback_(std::move(unsub_callback)),
      delegate_(this), m_isExpanded(false) {

    label_ = new QLabel(name_, this);
    expand_button_ = new QCollapseButton(this);
    expand_button_->setText("Expand");
    topics_view_ = new QTableView(this);
    topics_view_->setVisible(false);

    // Disable scrollbars
    topics_view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    topics_view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    topics_view_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    topics_view_->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
    topics_view_->setSelectionMode(QAbstractItemView::SelectionMode::NoSelection);
    topics_view_->setShowGrid(false);
    topics_view_->horizontalHeader()->setVisible(false);
    topics_view_->horizontalHeader()->setStretchLastSection(true);
    topics_view_->verticalHeader()->setVisible(false);

    // Set model and add topics
    topics_ = new QStandardItemModel(this);
    setTopics(topics);

    topics_view_->setModel(topics_);
    topics_view_->setItemDelegateForColumn(1, &delegate_);

    // Container for animation
    container_ = new QWidget(this);
    QVBoxLayout* listLayout = new QVBoxLayout(container_);
    listLayout->addWidget(topics_view_);
    listLayout->setContentsMargins(0, 0, 0, 0);
    container_->setLayout(listLayout);
    container_->setMaximumHeight(0);

    // Animation setup
    animation_ = new QPropertyAnimation(container_, "maximumHeight");
    animation_->setDuration(300);
    animation_->setEasingCurve(QEasingCurve::InOutQuad);

    // Layout
    main_layout_ = new QVBoxLayout(this);
    main_layout_->addWidget(label_);
    main_layout_->addWidget(expand_button_);
    main_layout_->addWidget(container_);
    main_layout_->setContentsMargins(0, 0, 0, 0);
    main_layout_->setSpacing(2);
    setLayout(main_layout_);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    connect(expand_button_, &QCollapseButton::clicked, this, &QSenderSubscriptions::toggleExpand);

    // Connect item change to subscription:
    connect(topics_, &QStandardItemModel::itemChanged, this, [&](QStandardItem* item) {
        const auto topic = topics_->item(item->index().row())->text().toStdString();
        const auto level = enum_cast<Level>(item->text().toStdString());
        if(level.has_value()) {
            sub_callback_(name_.toStdString(), topic, level.value());
        } else {
            unsub_callback_(name_.toStdString(), topic);
        }
    });
}

void QSenderSubscriptions::toggleExpand() {
    if(!m_isExpanded) {
        // Emit the signal to notify that this item has expanded
        emit expanded(this);
    }
    m_isExpanded = !m_isExpanded;
    updateExpansionHeight();
}

void QSenderSubscriptions::setTopics(const QStringList& topics) {

    // Remove old topics
    topics_->clear();
    topics_->setColumnCount(2);

    // Add new topics
    for(const auto& topic : topics) {
        // Underlying QStandardItemModel takes ownership of QStandardItem instances
        // NOLINTBEGIN(cppcoreguidelines-owning-memory)
        QList<QStandardItem*> row;
        row.append(new QStandardItem(topic));
        auto* item2 = new QStandardItem();
        row.append(item2);
        topics_->appendRow(row);
        topics_view_->openPersistentEditor(item2->index());
        // NOLINTEND(cppcoreguidelines-owning-memory)
    }

    // Recalculate height if expanded
    if(m_isExpanded) {
        updateExpansionHeight();
    }
}

void QSenderSubscriptions::updateExpansionHeight() {
    // Use current animation value as start
    animation_->setStartValue(animation_->currentValue());

    if(m_isExpanded) {
        int rowCount = topics_->rowCount();
        const int itemHeight = topics_view_->verticalHeader()->sectionSize(0);
        // int itemHeight = 20;
        int expandedHeight = rowCount * itemHeight + 10;

        topics_view_->setVisible(true);
        animation_->setEndValue(expandedHeight);
        expand_button_->setText("Collapse");
    } else {
        animation_->setEndValue(0);
        expand_button_->setText("Expand");
    }

    animation_->start();
}

// void QSenderSubscriptions::on_senderLevel_currentIndexChanged(int index) {
// const auto level = enum_cast<Level>(ui_->senderLevel->itemText(index).toStdString());
// if(level.has_value()) {
// sub_callback_(name_.toStdString(), "", level.value());
// } else {
// unsub_callback_(name_.toStdString(), "");
// }
// }
