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
#include "constellation/gui/QLogLevelComboBox.hpp"

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

    // Directly commit data to model when new item is selected - otherwise data is only committed when the editor loses focus
    connect(box, &QComboBox::currentIndexChanged, this, [this, box, parent]() {
        emit const_cast<ComboBoxItemDelegate*>(this)->commitData(box);
    });

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

QCollapseButton::QCollapseButton(const QString& text, QWidget* parent) : QToolButton(parent) {
    setCheckable(true);
    setStyleSheet("QToolButton { border-style: outset; border-width: 0px; font-weight: normal; }");
    setFont(QApplication::font());
    setArrowType(Qt::ArrowType::RightArrow);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);

    QToolButton::setText(" " + text);

    connect(this, &QToolButton::toggled, [&](bool checked) {
        setArrowType(checked ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
    });
}

QSenderSubscriptions::QSenderSubscriptions(QString name,
                                           std::function<void(const std::string&, const std::string&, Level)> sub_callback,
                                           std::function<void(const std::string&, const std::string&)> unsub_callback,
                                           const QStringList& topics,
                                           QWidget* parent)
    : QWidget(parent), name_(std::move(name)), sub_callback_(std::move(sub_callback)),
      unsub_callback_(std::move(unsub_callback)), delegate_(this) {

    expand_button_ = new QCollapseButton(name_, this);
    topics_view_ = new QTableView(this);
    topics_view_->setVisible(false);

    // Set model and add topics
    topics_ = new QStandardItemModel(this);
    setTopics(topics);
    topics_view_->setModel(topics_);
    topics_view_->setItemDelegateForColumn(1, &delegate_);

    // Disable scrollbars
    topics_view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    topics_view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // topics_view_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    topics_view_->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
    topics_view_->setSelectionMode(QAbstractItemView::SelectionMode::NoSelection);
    topics_view_->setShowGrid(false);

    topics_view_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    topics_view_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    topics_view_->horizontalHeader()->resizeSection(1, 100);
    topics_view_->horizontalHeader()->setVisible(false);
    topics_view_->verticalHeader()->setVisible(false);
    topics_view_->setFrameShape(QFrame::NoFrame);
    topics_view_->setStyleSheet("QTableView {background-color: transparent;}");

    // Sender log level
    sender_level_ = new QLogLevelComboBox(this);
    sender_level_->setDescending(false);
    sender_level_->addNeutralElement("- global -");

    // Container for animation
    container_ = new QWidget(this);
    QVBoxLayout* listLayout = new QVBoxLayout(container_);
    listLayout->addWidget(topics_view_);
    listLayout->setContentsMargins(25, 4, 0, 0);
    container_->setLayout(listLayout);
    container_->setMaximumHeight(0);

    // Animation setup
    animation_ = new QPropertyAnimation(container_, "maximumHeight");
    animation_->setDuration(300);
    animation_->setEasingCurve(QEasingCurve::InOutQuad);

    // Layout
    main_layout_ = new QGridLayout(this);
    main_layout_->addWidget(expand_button_, 0, 0, 1, 1);
    main_layout_->addWidget(sender_level_, 0, 1, 1, 1);
    main_layout_->addWidget(container_, 2, 0, 1, 2);
    main_layout_->setContentsMargins(0, 0, 0, 0);
    main_layout_->setSpacing(2);
    setLayout(main_layout_);

    connect(expand_button_, &QCollapseButton::toggled, this, [&](bool expand) {
        // Emit the signal to notify that this item has expanded or collapsed
        emit expanded(this, expand);
        update_height(expand);
    });

    // Connect the sender level to subscription:
    connect(sender_level_, &QLogLevelComboBox::currentTextChanged, this, [&](const QString& text) {
        const auto level = enum_cast<Level>(text.toStdString());
        if(level.has_value()) {
            sub_callback_(name_.toStdString(), "", level.value());
        } else {
            unsub_callback_(name_.toStdString(), "");
        }
    });

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

void QSenderSubscriptions::collapse() {
    expand_button_->setChecked(false);
    expand_button_->setArrowType(Qt::ArrowType::RightArrow);
    update_height(false);
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
    if(expand_button_->isChecked()) {
        update_height(true);
    }
}

void QSenderSubscriptions::update_height(bool expand) {
    // Use current animation value as start
    animation_->setStartValue(animation_->currentValue());

    if(expand) {
        const int rows = topics_->rowCount();
        const int item_height = topics_view_->verticalHeader()->sectionSize(0);
        const int expandedHeight = rows * item_height;
        topics_view_->setMinimumHeight(expandedHeight);

        topics_view_->setVisible(true);
        animation_->setEndValue(expandedHeight);
    } else {
        animation_->setEndValue(0);
    }

    animation_->start();
}
