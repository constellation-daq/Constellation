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
#include "constellation/gui/QCollapseButton.hpp"
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
    connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, box](int /* index */) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
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

void ComboBoxItemDelegate::updateEditorGeometry(QWidget* editor,
                                                const QStyleOptionViewItem& option,
                                                const QModelIndex& /*index*/) const {
    const int editor_width = editor->width();
    auto aligned_rect = option.rect;
    aligned_rect.setLeft(option.rect.right() - editor_width);
    aligned_rect.setWidth(editor_width);
    editor->setGeometry(aligned_rect);
}

// Qt takes care of cleanup since parent widgets are always specified:
// NOLINTBEGIN(cppcoreguidelines-owning-memory)

QSenderSubscriptions::QSenderSubscriptions(QString name,
                                           std::function<void(const std::string&, const std::string&, Level)> sub_callback,
                                           std::function<void(const std::string&, const std::string&)> unsub_callback,
                                           const QStringList& topics,
                                           QWidget* parent)
    : QWidget(parent), name_(std::move(name)), sub_callback_(std::move(sub_callback)),
      unsub_callback_(std::move(unsub_callback)), delegate_(this), sender_level_(new QLogLevelComboBox(this)),
      expand_button_(new QCollapseButton(name_, this)), topics_view_(new QTableView(this)),
      topics_(new QStandardItemModel(this)), container_(new QWidget(this)), main_layout_(new QGridLayout(this)) {

    topics_view_->setVisible(false);

    // Set model and add topics
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
    topics_view_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    topics_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    topics_view_->horizontalHeader()->setVisible(false);
    topics_view_->verticalHeader()->setVisible(false);
    topics_view_->setFrameShape(QFrame::NoFrame);
    topics_view_->setStyleSheet("QTableView {background-color: transparent;}");

    // Sender log level
    sender_level_->setDescending(false);
    sender_level_->addNeutralElement("- global -");

    // Container for animation
    auto* listLayout = new QVBoxLayout(container_);
    listLayout->addWidget(topics_view_);
    listLayout->setContentsMargins(25, 4, 0, 0);
    container_->setLayout(listLayout);
    container_->setMaximumHeight(0);

    // Animation setup
    animation_ = new QPropertyAnimation(container_, "maximumHeight");
    animation_->setDuration(300);
    animation_->setEasingCurve(QEasingCurve::InOutQuad);

    // Layout
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
        const auto type = name_.section('.', 0, 0).toUpper().toStdString();
        if(level.has_value()) {
            sub_callback_(name_.toStdString(), type, level.value());
        } else {
            unsub_callback_(name_.toStdString(), type);
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

// NOLINTEND(cppcoreguidelines-owning-memory)

void QSenderSubscriptions::collapse() {
    expand_button_->setChecked(false);
    expand_button_->setArrowType(Qt::ArrowType::RightArrow);
    update_height(false);
}

void QSenderSubscriptions::setTopics(const QStringList& topics) {

    // Loop over received topic list and add all new ones:
    for(const auto& topic : topics) {
        // Skip the type topic:
        if(topic == name_.section('.', 0, 0).toUpper()) {
            continue;
        }

        if(topics_->findItems(topic).empty()) {
            // Underlying QStandardItemModel takes ownership of QStandardItem instances
            // NOLINTBEGIN(cppcoreguidelines-owning-memory)
            QList<QStandardItem*> row;
            row.append(new QStandardItem(topic));
            auto* item2 = new QStandardItem();
            item2->setTextAlignment(Qt::AlignRight);
            row.append(item2);
            topics_->appendRow(row);
            topics_view_->openPersistentEditor(item2->index());
            // NOLINTEND(cppcoreguidelines-owning-memory)
        }
    }

    // Loop over existing topics and remove the ones not in the received list anymore
    // Using reverse order to keep row indices valid
    for(int row = topics_->rowCount() - 1; row >= 0; --row) {
        const auto index = topics_->index(row, 0);
        const auto topic = topics_->data(index).toString();
        if(!topics.contains(topic)) {
            topics_->removeRow(row);
        }
    }

    // Sort the new list of topics:
    topics_->sort(0);

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
