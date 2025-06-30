/**
 * @file
 * @brief TelemetryConsole GUI
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "TelemetryConsole.hpp"

#include <QCloseEvent>
#include <QDateTime>
#include <QList>
#include <QSplitter>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "constellation/build.hpp"

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

    // Start the log receiver pool
    stat_listener_.startPool();

    // Central dashboard widget with scroll area to hold widgets
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(&dashboard_widget_);
}

void TelemetryConsole::onAddMetric() {

    const auto sender = metricSender->currentText();
    const auto type = metricType->currentText();
    const auto metric = metricName->currentText();

    if(metric.isEmpty() || sender.isEmpty()) {
        return;
    }

    // Create new metric widget
    auto* metric_widget = create_metric_display(
        sender, metric, type, checkBoxWindow->isChecked(), static_cast<std::size_t>(spinBoxMins->value() * 60));

    if(metric_widget == nullptr) {
        return;
    }

    // Subscribe to metric
    stat_listener_.subscribeMetric(sender.toStdString(), metric.toStdString());

    // Connect delete request and update method
    connect(metric_widget, &QMetricDisplay::deleteRequested, this, &TelemetryConsole::onDeleteMetricWidget);
    connect(&stat_listener_, &QStatListener::newMessage, metric_widget, &QMetricDisplay::update);

    // Store widget and update layout
    metric_widgets_.append(metric_widget);
    update_layout();

    // Clear inputs for next metric to be added
    metricName->clear();
    metricSender->setCurrentIndex(-1);
}

void TelemetryConsole::onDeleteMetricWidget() {
    // Get object which sent this signal
    auto* metric_widget = qobject_cast<QMetricDisplay*>(sender());
    if(!metric_widget) {
        return;
    }

    // Unsubscribe from metric
    stat_listener_.unsubscribeMetric(metric_widget->getSender().toStdString(), metric_widget->getMetric().toStdString());

    // Remove from list of metrics
    metric_widgets_.removeOne(metric_widget);

    // Mark for deletion and update layout
    metric_widget->deleteLater();
    update_layout();
}

void TelemetryConsole::onDeleteMetricWidgets() {

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
    // Call the clear method of all widgets
    for(auto& metric : metric_widgets_) {
        metric->clear();
    }
}

void TelemetryConsole::update_layout() {
    if(metric_widgets_.empty()) {
        return;
    }

    // Delete old layout and remove child widgets
    if(auto* old_layout = dashboard_widget_.layout()) {
        delete old_layout;
    }

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

    const auto count = metric_widgets_.size();
    int optimal_cols = 1;
    int optimal_rows = count;

    // Find optimal number of columns
    for(int cols = 1; cols <= count; ++cols) {
        const auto rows = (count + cols - 1) / cols; // ceil division
        const auto empty_cells = cols * rows - count;

        // Difference to target ratio
        const auto ratio_difference = std::abs(static_cast<double>(cols) / rows - target_ratio);

        // Final score consisting of ratio difference and penalties for empty cells
        const auto score = ratio_difference + empty_cells * penalty_weight;
        if(score < best_score) {
            best_score = score;
            optimal_cols = cols;
            optimal_rows = rows;
        }
    }

    // Ownership is transferred to the dashboard widget
    // NOLINTBEGIN(cppcoreguidelines-owning-memory)

    // Generate splitters to allow resizing
    auto* splitter_vertical = new QSplitter(Qt::Vertical, &dashboard_widget_);

    int widget_idx = 0;
    for(int row = 0; row < optimal_rows; ++row) {
        // Split row vertically
        auto* splitter_horizontal = new QSplitter(Qt::Horizontal, splitter_vertical);

        for(int col = 0; col < optimal_cols; ++col) {
            auto* w = (widget_idx < count ? metric_widgets_[widget_idx++] : new QWidget(splitter_horizontal));
            w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            splitter_horizontal->addWidget(w);
        }

        // Enforce equal column widths in this row
        splitter_horizontal->setSizes(QList<int>(optimal_cols, 1));
        splitter_vertical->addWidget(splitter_horizontal);
    }

    // Enforce equal row heights
    splitter_vertical->setSizes(QList<int>(optimal_rows, 1));

    auto* layout = new QVBoxLayout(&dashboard_widget_);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(splitter_vertical);
    dashboard_widget_.setLayout(layout);
    // NOLINTEND(cppcoreguidelines-owning-memory)
}

QMetricDisplay* TelemetryConsole::create_metric_display(
    const QString& sender, const QString& name, const QString& type, bool window, std::size_t seconds) {

    // Ownership is transferred to the caller
    // NOLINTBEGIN(cppcoreguidelines-owning-memory)
    if(type == "Spline") {
        return new QSplineMetricDisplay(sender, name, window, seconds, this);
    } else if(type == "Scatter") {
        return new QScatterMetricDisplay(sender, name, window, seconds, this);
    } else if(type == "Area") {
        return new QAreaMetricDisplay(sender, name, window, seconds, this);
    }
    // NOLINTEND(cppcoreguidelines-owning-memory)

    return nullptr;
}

void TelemetryConsole::closeEvent(QCloseEvent* event) {
    // Stop the stat receiver
    stat_listener_.stopPool();

    // Terminate the application
    event->accept();
}
