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
#include <mutex>
#include <optional>
#include <string_view>

#include <QCloseEvent>
#include <QList>
#include <QMainWindow>
#include <QSettings>
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

private slots:
    /**
     * @brief Slot for adding a new metric to the dashboard
     * @details Creates the metric display and handles subscriptions
     */
    void create_metric();

    /**
     * @brief Slot for adding a new sender to an existing metric on the dashboard
     * @details Adds the sender to the metric display and handles subscriptions
     */
    void add_metric();

    /**
     * @brief Slot for clearing the content of all metric widgets
     * @details This clears stored data points
     */
    void reset_metric();

    /**
     * @brief Slot for deleting a metric widget
     * @details This removes the widget, unsubscribes from the metric and updates the dashboard layout
     */
    void delete_metric();

    /**
     * @brief Slot for deleting all metric widgets from the dashboard
     * @details Clears widgets, deletes them and unsubscribes from metrics
     */
    void delete_all_metrics();

    /**
     * @brief Slot for restoring metric widgets from the application settings
     * @details Reads the application settings and restores all widgets and the layout
     */
    void restore_metrics();

    /**
     * @brief Helper to update the dashboard layout after adding or removing a widget
     * @details This calculates best position of widgets and calls generate_splitters to rearrange them
     */
    void update_layout();

private:
    // Telemetry listener
    QStatListener stat_listener_;

    /**
     * @brief Helper to generate the dashboard layout
     * @details Takes splitter sizes vertically and horizontally and builds the grid from it. Removes the old layout before
     *          adding the new one
     *
     * @param vertical Relative sizes of the vertical sections (rows)
     * @param horizontal Relative sizes for the horizontal cells in each of the rows
     */
    void generate_splitters(const QList<int>& vertical, const QVector<QList<int>>& horizontal);

    /**
     * @brief Helper to create different types of metric display widgets
     *
     * @param name Name of the metric
     * @param type_name Type of chart to book
     * @param window Optional number of seconds for a sliding window
     */
    void create_metric_display(const QString& name, const QString& type_name, std::optional<std::size_t> window);

    /**
     * @brief Helper to attach a sender to a metric display
     *
     * @param name Name of the metric
     * @param sender Name of the sender
     */
    void add_metric_sender(const QString& name, const QString& sender);

    // Vector of all metric widgets
    QVector<QMetricDisplay*> metric_widgets_;
    std::mutex metric_widgets_mutex_;

    // Qt widget to display dashboard
    QWidget dashboard_widget_;

    /** UI Settings */
    QSettings gui_settings_;
};
