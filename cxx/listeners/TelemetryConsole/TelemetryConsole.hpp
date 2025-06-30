/**
 * @file
 * @brief Telemetry Console GUI
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <string_view>

#include <QCloseEvent>
#include <QMainWindow>
#include <QVector>
#include <QWidget>

#include "QMetricDisplay.hpp"
#include "QStatListener.hpp"
#include "ui_TelemetryConsole.h"

/**
 * \class TelemetryConsole
 * \brief Main window of the Telemetry Console metric UI
 * @details This class implements the Qt QMainWindow component of the Telemetry Console metrics UI, connects signals to
 * the slots of different UI elements and takes care of the dashboard layout.
 */
class TelemetryConsole : public QMainWindow, public Ui::TelemetryConsole {
    Q_OBJECT
public:
    /**
     * @brief TelemetryConsole Constructor
     *
     * @param group_name Constellation group name to connect to
     */
    explicit TelemetryConsole(std::string_view group_name);

public:
    /**
     * @brief Qt QCloseEvent handler which stores the UI settings to file
     *
     * @param event The Qt close event
     */
    void closeEvent(QCloseEvent* event) override;

public slots:
    /**
     * @brief Slot for adding a new metric to the dashboard
     * @details Creates the metric display and handles subscriptions
     */
    void onAddMetric();

    /**
     * @brief Slot for clearing the content of all metric widgets
     * @details This clears stored data points
     */
    void onResetMetricWidgets();

    /**
     * @brief Slot for deleting a metric widget
     * @details This removes the widget, unsubscribes from the metric and updates the dashboard layout
     */
    void onDeleteMetricWidget();

    /**
     * @brief [Slot for deleting all metric widgets from the dashboard
     * @details Clears widgets, deletes them and unsubscribes from metrics
     */
    void onDeleteMetricWidgets();

private:
    // Telemetry listener
    QStatListener stat_listener_;

    /**
     * @brief Helper to update the dashboard layout after adding or removing a widget
     * @details This calculates best position of widgets and rearranges them
     */
    void update_layout();

    /**
     * @brief Helper to create different types of metric display widgets
     *
     * @param sender Name of the sender
     * @param name Name of the metric
     * @param type Type of chart to book
     * @param window Boolean indicating whether this is a sliding window display
     * @param seconds Number of seconds the sliding window extends
     * @return Widget
     */
    QMetricDisplay*
    create_metric_display(const QString& sender, const QString& name, const QString& type, bool window, std::size_t seconds);

    // Vector of all metric widgets
    QVector<QMetricDisplay*> metric_widgets_;

    // Qt widget to display dashboard
    QWidget dashboard_widget_;
};
