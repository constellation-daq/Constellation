/**
 * @file
 * @brief TelemetryConsole GUI
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "TelemetryConsole.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>

#include <QCloseEvent>
#include <QDateTime>
#include <QList>
#include <QSplitter>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "QMetricDisplay.hpp"
#include "QStatListener.hpp"

TelemetryConsole::TelemetryConsole(std::string_view group_name) {

    setupUi(this);

    // Set up header bar:
    cnstlnName->setText(QString::fromStdString("<font color=gray><b>" + std::string(group_name) + "</b></font>"));

    spinBoxMins->setEnabled(false);

    connect(addMetric, &QPushButton::clicked, this, &TelemetryConsole::onAddMetric);
    connect(resetMetrics, &QPushButton::clicked, this, &TelemetryConsole::onResetMetricWidgets);
    connect(clearMetrics, &QPushButton::clicked, this, &TelemetryConsole::onDeleteMetricWidgets);
    connect(checkBoxWindow, &QCheckBox::toggled, spinBoxMins, &QSpinBox::setEnabled);

    // When selecting new satellite, update available metric list
    connect(metricSender, &QComboBox::currentTextChanged, this, [&](const QString& text) {
        metricName->clear();
        for(const auto& [topic, desc] : stat_listener_.getAvailableTopics(text.toStdString())) {
            metricName->addItem(QString::fromStdString(topic));
        }
    });

    // When sender connected or disconnected, refresh satellite list and sender count
    connect(&stat_listener_, &QStatListener::connectionsChanged, this, [&](std::size_t num) {
        labelNrSatellites->setText("<font color='gray'><b>" + QString::number(num) + "</b></font>");

        metricSender->clear();
        for(const auto& sender : stat_listener_.getAvailableSenders()) {
            metricSender->addItem(QString::fromStdString(sender));
        }
        metricSender->setCurrentIndex(-1);
    });

    // Update existing metric widgets with connection updates:
    connect(&stat_listener_, &QStatListener::senderConnected, this, [&](const QString& host) {
        const std::lock_guard widgets_lock {metric_widgets_mutex_};
        for(auto& metric : metric_widgets_) {
            if(metric->getSender() == host) {
                metric->setConnection(true);
            }
        }
    });
    connect(&stat_listener_, &QStatListener::senderDisconnected, this, [&](const QString& host) {
        const std::lock_guard widgets_lock {metric_widgets_mutex_};
        for(auto& metric : metric_widgets_) {
            if(metric->getSender() == host) {
                metric->setConnection(false);
            }
        }
    });

    // Connect restore placeholder button to restore widgets from settings:
    connect(restoreButton, &QPushButton::clicked, this, [&]() {
        gui_settings_.beginGroup("dashboard");
        const auto cnt = gui_settings_.value("count", 0).toInt();
        for(int i = 0; i < cnt; ++i) {
            gui_settings_.beginGroup(QString("widget%1").arg(i));
            const auto sender = gui_settings_.value("sender").toString();
            const auto type = gui_settings_.value("type").toString();
            const auto metric = gui_settings_.value("metric").toString();

            bool sliding_window = false;
            std::size_t duration = 0;
            const auto slide = gui_settings_.value("slide");
            if(slide.isValid()) {
                sliding_window = true;
                duration = static_cast<std::size_t>(slide.toInt());
            }
            gui_settings_.endGroup();

            // Drop prefix and suffix from type
            create_metric_display(sender, metric, type.mid(1, type.size() - 14), sliding_window, duration);
        }
        gui_settings_.endGroup();
    });

    // Start the log receiver pool
    stat_listener_.startPool();

    // Central dashboard widget with scroll area to hold widgets
    scrollArea->setWidgetResizable(true);

    // Restore window geometry:
    restoreGeometry(gui_settings_.value("window/geometry", saveGeometry()).toByteArray());
    restoreState(gui_settings_.value("window/savestate", saveState()).toByteArray());
    move(gui_settings_.value("window/pos", pos()).toPoint());
    resize(gui_settings_.value("window/size", size()).toSize());
    if(gui_settings_.value("window/maximized", isMaximized()).toBool()) {
        showMaximized();
    }
}

void TelemetryConsole::onAddMetric() {

    const auto sender = metricSender->currentText();
    const auto type = metricType->currentText();
    const auto metric = metricName->currentText();

    if(metric.isEmpty() || sender.isEmpty()) {
        return;
    }

    // Create new metric widget
    create_metric_display(
        sender, metric, type, checkBoxWindow->isChecked(), static_cast<std::size_t>(spinBoxMins->value()) * 60);

    // Clear inputs for next metric to be added
    metricName->clear();
    metricSender->setCurrentIndex(-1);
}

void TelemetryConsole::onDeleteMetricWidget() {
    std::unique_lock widgets_lock {metric_widgets_mutex_};

    // Get object which sent this signal
    auto* metric_widget = qobject_cast<QMetricDisplay*>(sender());
    if(metric_widget == nullptr) {
        return;
    }

    // Unsubscribe from metric
    stat_listener_.unsubscribeMetric(metric_widget->getSender().toStdString(), metric_widget->getMetric().toStdString());

    // Remove from list of metrics
    metric_widgets_.removeOne(metric_widget);

    // Mark for deletion and update layout
    metric_widget->deleteLater();
    widgets_lock.unlock();

    update_layout();
}

void TelemetryConsole::onDeleteMetricWidgets() {
    const std::lock_guard widgets_lock {metric_widgets_mutex_};

    // Loop over all metric widgets, remove them from layout, unsubscribe and mark for deletion
    for(auto& metric : metric_widgets_) {
        stat_listener_.unsubscribeMetric(metric->getSender().toStdString(), metric->getMetric().toStdString());
        metric_widgets_.removeOne(metric);
        metric->deleteLater();
    }

    // Update dashboard layout
    update_layout();
}

void TelemetryConsole::onResetMetricWidgets() {
    const std::lock_guard widgets_lock {metric_widgets_mutex_};

    // Call the reset method of all widgets
    for(auto& metric : metric_widgets_) {
        metric->reset();
    }
}

void TelemetryConsole::update_layout() {
    const std::lock_guard widgets_lock {metric_widgets_mutex_};

    if(metric_widgets_.empty()) {
        return;
    }

    // Delete old layout and remove child widgets
    delete dashboard_widget_.layout();

    // Single widget occupies entire area
    if(metric_widgets_.size() == 1) {
        // Ownership is transferred to the dashboard widget
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        auto* layout = new QVBoxLayout(&dashboard_widget_);
        layout->setContentsMargins(0, 0, 0, 0);
        metric_widgets_[0]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(metric_widgets_[0]);
        dashboard_widget_.setLayout(layout);
        return;
    }

    // Multiple widgets are arranged in a grid with ration 3/2 with penalty-based position calculation
    constexpr double target_ratio = 3.0 / 2.0;
    constexpr double penalty_weight = 0.5;
    auto best_score = std::numeric_limits<double>::max();

    const auto count = static_cast<std::size_t>(metric_widgets_.size());
    std::size_t optimal_cols = 1;
    std::size_t optimal_rows = count;

    // Find optimal number of columns
    for(std::size_t cols = 1; cols <= count; ++cols) {
        const auto rows = (count + cols - 1) / cols;
        const auto empty_cells = (cols * rows) - count;

        // Difference to target ratio
        const auto ratio_difference = std::abs((static_cast<double>(cols) / static_cast<double>(rows)) - target_ratio);

        // Final score consisting of ratio difference and penalties for empty cells
        const auto score = ratio_difference + (static_cast<double>(empty_cells) * penalty_weight);
        if(score < best_score) {
            best_score = score;
            optimal_cols = cols;
            optimal_rows = rows;
        }
    }

    // Generate list with relative width - Qt5 is missing the constructor, setSizes ignores additional entries
    QList<int> cell_sizes;
    for(std::size_t r = 0; r < std::max(optimal_rows, optimal_cols); r++) {
        cell_sizes.append(1);
    }

    // Ownership is transferred to the dashboard widget
    // NOLINTBEGIN(cppcoreguidelines-owning-memory)

    // Generate splitters to allow resizing
    auto* splitter_vertical = new QSplitter(Qt::Vertical, &dashboard_widget_);
    splitter_vertical->setHandleWidth(9);

    int widget_idx = 0;
    for(std::size_t row = 0; row < optimal_rows; ++row) {
        // Split row vertically
        auto* splitter_horizontal = new QSplitter(Qt::Horizontal, splitter_vertical);
        splitter_horizontal->setHandleWidth(9);

        for(std::size_t col = 0; col < optimal_cols; ++col) {
            auto* w =
                (widget_idx < static_cast<int>(count) ? metric_widgets_[widget_idx++] : new QWidget(splitter_horizontal));
            w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            splitter_horizontal->addWidget(w);
        }

        // Enforce equal column widths in this row
        splitter_horizontal->setSizes(cell_sizes);
        splitter_vertical->addWidget(splitter_horizontal);
    }

    // Enforce equal row heights
    splitter_vertical->setSizes(cell_sizes);

    auto* layout = new QVBoxLayout(&dashboard_widget_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(splitter_vertical);
    dashboard_widget_.setLayout(layout);
    // NOLINTEND(cppcoreguidelines-owning-memory)
}

void TelemetryConsole::create_metric_display(
    const QString& sender, const QString& name, const QString& type, bool window, std::size_t seconds) {

    // Ensure we are displaying the dashboard:
    scrollArea->setWidget(&dashboard_widget_);

    // Ownership is transferred to the storage
    // NOLINTBEGIN(cppcoreguidelines-owning-memory)
    QMetricDisplay* metric_widget = nullptr;
    if(type == "Spline") {
        metric_widget = new QSplineMetricDisplay(sender, name, window, seconds, this);
    } else if(type == "Scatter") {
        metric_widget = new QScatterMetricDisplay(sender, name, window, seconds, this);
    } else if(type == "Area") {
        metric_widget = new QAreaMetricDisplay(sender, name, window, seconds, this);
    }
    // NOLINTEND(cppcoreguidelines-owning-memory)

    if(metric_widget == nullptr) {
        return;
    }

    // Set connection state of the widget:
    if(!stat_listener_.isSenderAvailable(sender.toStdString())) {
        metric_widget->setConnection(false);
    }

    std::unique_lock widgets_lock {metric_widgets_mutex_};
    // Subscribe to metric
    stat_listener_.subscribeMetric(sender.toStdString(), name.toStdString());

    // Connect delete request and update method
    connect(metric_widget, &QMetricDisplay::deleteRequested, this, &TelemetryConsole::onDeleteMetricWidget);
    connect(&stat_listener_, &QStatListener::newMessage, metric_widget, &QMetricDisplay::update);

    // Store widget and update layout
    metric_widgets_.append(metric_widget);

    widgets_lock.unlock();
    update_layout();
}

void TelemetryConsole::closeEvent(QCloseEvent* event) {
    // Stop the stat receiver
    stat_listener_.stopPool();

    // Store current metric widgets:
    gui_settings_.beginGroup("dashboard");

    std::unique_lock widgets_lock {metric_widgets_mutex_};
    gui_settings_.setValue("count", metric_widgets_.size());
    for(int i = 0; i < metric_widgets_.size(); ++i) {
        gui_settings_.beginGroup(QString("widget%1").arg(i));
        gui_settings_.setValue("type", metric_widgets_[i]->metaObject()->className());
        gui_settings_.setValue("sender", metric_widgets_[i]->getSender());
        gui_settings_.setValue("metric", metric_widgets_[i]->getMetric());
        const auto sliding = metric_widgets_[i]->slidingWindow();
        if(sliding.has_value()) {
            gui_settings_.setValue("slide", static_cast<int>(sliding.value()));
        }
        gui_settings_.endGroup();
    }
    gui_settings_.endGroup();
    widgets_lock.unlock();

    // Store window geometry:
    gui_settings_.setValue("window/geometry", saveGeometry());
    gui_settings_.setValue("window/savestate", saveState());
    gui_settings_.setValue("window/maximized", isMaximized());
    if(!isMaximized()) {
        gui_settings_.setValue("window/pos", pos());
        gui_settings_.setValue("window/size", size());
    }

    // Terminate the application
    event->accept();
}
