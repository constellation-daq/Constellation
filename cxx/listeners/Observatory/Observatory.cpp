/**
 * @file
 * @brief Observatory GUI
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "Observatory.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <QBrush>
#include <QCloseEvent>
#include <QDateTime>
#include <QMetaType>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTreeWidgetItem>

#include "constellation/build.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/string.hpp"
#include "constellation/gui/QLogMessage.hpp"
#include "constellation/gui/QLogMessageDialog.hpp"
#include "constellation/gui/qt_utils.hpp"

#include "QLogListener.hpp"
#include "QSubscriptionList.hpp"

using namespace constellation::gui;
using namespace constellation::log;
using namespace constellation::utils;

LogStatusBar::LogStatusBar()
    : layout_(this), label_all_(new QLabel("0 messages")), label_critical_(new QLabel()), label_warning_(new QLabel()) {

    label_all_->setStyleSheet("QLabel { font-size: 12px; font-weight: normal; color: gray; }");
    label_critical_->setStyleSheet("QLabel { font-size: 12px; font-weight: bold; color: red; }");
    label_warning_->setStyleSheet("QLabel { font-size: 12px; font-weight: bold; color: orange; }");

    layout_.addWidget(label_critical_);
    layout_.addWidget(label_warning_);
    layout_.addWidget(label_all_);
}

void LogStatusBar::resetMessageCounts() {
    msg_all_ = 0;
    msg_warning_ = 0;
    msg_critical_ = 0;

    label_all_->setText(QString::number(msg_all_) + " messages");
    label_warning_->setText("");
    label_critical_->setText("");
}

void LogStatusBar::countMessage(Level level) {

    label_all_->setText(QString::number(++msg_all_) + " messages");

    if(level == Level::WARNING) {
        label_warning_->setText(QString::number(++msg_warning_) + " warnings");
    }

    if(level == Level::CRITICAL) {
        label_critical_->setText(QString::number(++msg_critical_) + " errors");
    }
}

void LogItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    auto options = option;

    // Get sibling for column 2 (where the log level is stored) for current row:
    const QModelIndex lvl_index = index.sibling(index.row(), 2);

    // Get log level color
    const auto level_str = lvl_index.data().toString().toStdString();
    const auto level = enum_cast<Level>(level_str).value_or(WARNING);

    const auto color = get_log_level_color(level);
    if(level > Level::INFO) {
        // High levels get background coloring
        painter->fillRect(options.rect, QBrush(color));
    } else {
        // Others just text color adjustment
        options.palette.setColor(QPalette::Text, color);
    }

    QStyledItemDelegate::paint(painter, options, index);
}

QString LogItemDelegate::displayText(const QVariant& value, const QLocale& locale) const {
    if(value.userType() == QMetaType::QDateTime) {
        return locale.toString(value.toDateTime().toLocalTime(), "yyyy-MM-dd hh:mm:ss");
    }
    return QStyledItemDelegate::displayText(value, locale);
}

Observatory::Observatory(std::string_view group_name) : logger_("UI") {

    qRegisterMetaType<QModelIndex>("QModelIndex");
    setupUi(this);

    // Qt UI has to be initialized before we can call this, and it takes ownership of the pointer
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,cppcoreguidelines-prefer-member-initializer)
    subscription_list_widget_ = new QSubscriptionList(subscriptionsIndividual);
    subscriptionLayout->addWidget(subscription_list_widget_);

    setWindowTitle("Constellation Observatory " CNSTLN_VERSION_FULL);

    // Connect signals:
    connect(&log_listener_, &QLogListener::senderConnected, this, [&](const QString& sender) {
        // Only add if not listed yet
        if(filterSender->findText(sender) < 0) {
            filterSender->addItem(sender);
        }
    });
    connect(&log_listener_, &QLogListener::newGlobalTopics, this, [&](const QStringList& topics) {
        filterTopic->clear();
        filterTopic->addItem("- All -");
        filterTopic->addItems(topics);
    });
    connect(&log_listener_, &QLogListener::connectionsChanged, this, [&](std::size_t num) {
        labelNrSatellites->setText("<font color='gray'><b>" + QString::number(num) + "</b></font>");
    });
    connect(&log_listener_, &QLogListener::newSenderTopics, this, [&](const QString& sender, const QStringList& topics) {
        subscription_list_widget_->setTopics(sender, topics);
    });
    connect(&log_listener_, &QLogListener::senderConnected, this, [&](const QString& host) {
        subscription_list_widget_->addHost(host, log_listener_);
    });
    connect(&log_listener_, &QLogListener::senderDisconnected, this, [&](const QString& sender) {
        subscription_list_widget_->removeHost(sender);
    });

    // Start the log receiver pool
    log_listener_.startPool();

    // Set up header bar:
    cnstlnName->setText(QString::fromStdString("<font color=gray><b>" + std::string(group_name) + "</b></font>"));

    log_filter_.setSourceModel(&log_listener_);
    viewLog->setModel(&log_filter_);
    viewLog->setItemDelegate(&log_message_delegate_);
    for(int col = 0; col < QLogMessage::countColumns(); ++col) {
        const int width = QLogMessage::columnWidth(col);
        if(width >= 0) {
            viewLog->setColumnWidth(col, width);
        }
    }
    // Enable uniform row height to allow for optimizations on Qt end:
    viewLog->setUniformRowHeights(true);
    filterLevel->setDescending(true);

    // Restore window geometry:
    restoreGeometry(gui_settings_.value("window/geometry", saveGeometry()).toByteArray());
    restoreState(gui_settings_.value("window/savestate", saveState()).toByteArray());
    move(gui_settings_.value("window/pos", pos()).toPoint());
    resize(gui_settings_.value("window/size", size()).toSize());
    if(gui_settings_.value("window/maximized", isMaximized()).toBool()) {
        showMaximized();
    }

    // Load last filter settings:
    if(gui_settings_.contains("filters/level")) {
        const auto qlevel = gui_settings_.value("filters/level").toString();
        const auto level = enum_cast<Level>(qlevel.toStdString());
        log_filter_.setFilterLevel(level.value_or(Level::TRACE));
        filterLevel->setCurrentIndex(std::to_underlying(level.value_or(Level::TRACE)));
    }
    if(gui_settings_.contains("filters/sender")) {
        const auto sender = gui_settings_.value("filters/sender").toString();
        log_filter_.setFilterSender(sender.toStdString());
        filterSender->setCurrentText(QString::fromStdString(log_filter_.getFilterSender()));
    }
    if(gui_settings_.contains("filters/topic")) {
        const auto topic = gui_settings_.value("filters/topic").toString();
        log_filter_.setFilterTopic(topic.toStdString());
        filterTopic->setCurrentText(QString::fromStdString(log_filter_.getFilterTopic()));
    }
    const auto pattern = gui_settings_.value("filters/search", "");
    log_filter_.setFilterMessage(pattern.toString());
    filterMessage->setText(pattern.toString());

    // Load last subscription:
    const auto qslevel = gui_settings_.value("subscriptions/level").toString();
    const auto slevel = enum_cast<Level>(qslevel.toStdString());
    log_listener_.setGlobalLogLevel(slevel.value_or(Level::WARNING));
    globalLevel->setCurrentLevel(slevel.value_or(Level::WARNING));

    // Set up status bar:
    statusBar()->addPermanentWidget(&status_bar_);
    connect(
        &log_listener_, &QLogListener::newMessage, this, [&](QModelIndex, Level level) { status_bar_.countMessage(level); });
    statusBar()->showMessage("Startup Complete", 2000);
}

void Observatory::closeEvent(QCloseEvent* event) {
    // Stop the log receiver
    log_listener_.stopPool();

    // Store window geometry:
    gui_settings_.setValue("window/geometry", saveGeometry());
    gui_settings_.setValue("window/savestate", saveState());
    gui_settings_.setValue("window/maximized", isMaximized());
    if(!isMaximized()) {
        gui_settings_.setValue("window/pos", pos());
        gui_settings_.setValue("window/size", size());
    }

    // Store filter settings
    gui_settings_.setValue("filters/level", QString::fromStdString(to_string(log_filter_.getFilterLevel())));
    gui_settings_.setValue("filters/sender", QString::fromStdString(log_filter_.getFilterSender()));
    gui_settings_.setValue("filters/topic", QString::fromStdString(log_filter_.getFilterTopic()));
    gui_settings_.setValue("filters/search", log_filter_.getFilterMessage());

    // Store subscription settings
    gui_settings_.setValue("subscriptions/level", QString::fromStdString(to_string(log_listener_.getGlobalLogLevel())));

    // Terminate the application
    event->accept();
}

void Observatory::on_filterLevel_currentIndexChanged(int index) {
    log_filter_.setFilterLevel(Level(index));
}

void Observatory::on_globalLevel_currentIndexChanged(int index) {
    const auto level = enum_cast<Level>(globalLevel->itemText(index).toStdString());
    log_listener_.setGlobalLogLevel(level.value_or(Level::WARNING));
}

void Observatory::on_filterSender_currentTextChanged(const QString& text) {
    log_filter_.setFilterSender(text.toStdString());
}

void Observatory::on_filterTopic_currentTextChanged(const QString& text) {
    log_filter_.setFilterTopic(text.toStdString());
}

void Observatory::on_filterMessage_editingFinished() {
    log_filter_.setFilterMessage(filterMessage->displayText());
}

void Observatory::on_viewLog_activated(const QModelIndex& i) {
    // Translate to source index:
    const QModelIndex index = log_filter_.mapToSource(i);
    new QLogMessageDialog(this, log_listener_.getMessage(index));
}

void Observatory::on_clearFilters_clicked() {
    // Reset all filters
    filterLevel->setCurrentIndex(0);
    filterSender->setCurrentIndex(0);
    filterTopic->setCurrentIndex(0);

    // Setting the text does not emit the editingFinished signal, do it manually
    filterMessage->setText("");
    log_filter_.setFilterMessage("");
}

void Observatory::on_clearMessages_clicked() {
    // Clear all messages
    log_listener_.clearMessages();

    // Reset and add available senders from the connected ones and reset to all
    filterSender->clear();
    filterSender->addItem("- All -");
    for(const auto& sender : log_listener_.getAvailableSenders()) {
        filterSender->addItem(QString::fromStdString(sender));
    }
    log_filter_.setFilterSender("- All -");

    // Reset and add available topics
    QStringList topics;
    for(const auto& [topic, desc] : log_listener_.getAvailableTopics()) {
        topics.append(QString::fromStdString(topic));
    }
    topics.removeDuplicates();
    topics.sort();

    filterTopic->clear();
    filterTopic->addItem("- All -");
    filterTopic->addItems(topics);
    log_filter_.setFilterTopic("- All -");

    status_bar_.resetMessageCounts();
}
