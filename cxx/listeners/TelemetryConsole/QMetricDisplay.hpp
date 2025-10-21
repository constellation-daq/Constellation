/**
 * @file
 * @brief Metric Chart Display Widget
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMap>
#include <QString>
#include <QToolButton>
#include <QVariant>
#include <QWidget>

#include "QMetricSeries.hpp"
#include <QtCharts/QAbstractSeries>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#define QT_CHART QtCharts::
#else
#define QT_CHART
#endif

/**
 * @class QMetricDisplay
 * @brief Base class for displaying an auto-updating chart view of a given metric
 * @details The metric values are added via the update slot, the widget provides buttons for requesting its deletion as well
 *          as for resetting of the displayed data
 */
class QMetricDisplay : public QFrame {
    Q_OBJECT

public:
    /**
     * @brief Type of chart to create
     */
    enum class Type : std::uint8_t { Spline, Scatter, Area };

    /**
     * @brief Constructor of QMetricDisplay
     *
     * @param metric Name of the metric
     * @param type Type of metric series
     * @param window Window Optional size for sliding window display, no sliding window if `nullopt`
     * @param parent Parent QWidget
     */
    QMetricDisplay(const QString& metric, Type type, std::optional<std::size_t> window, QWidget* parent = nullptr);

    /**
     * @brief Reset method
     * @details Clears the data of all series
     */
    void reset();

    /**
     * @brief Register a new sender to add a data series for
     * @details This generates a new series of the configured type for the sender
     *
     * @param sender Canonical name of the sender
     */
    void addSender(const QString& sender);

    /**
     * @brief Check if sender registered for this widget
     * @return True if sender is registered, false otherwise
     */
    bool hasSender(const QString& sender) const { return series_.contains(sender); }

    /**
     * @brief Obtain list of registered senders this chart displays
     * @return List of registered senders
     */
    QStringList getSenders() const { return series_.keys(); }

    /**
     * @brief Obtain type of chart display
     * @return Type of chart display
     */
    Type getType() const { return type_; }

    /**
     * @brief Get metric registered for this widget
     * @return Name of the metric
     */
    QString getMetric() const { return metric_; }

    /**
     * @brief Get sliding window length
     * @details Only holds a value if sliding window is active
     * @return Optional with sliding window length
     */
    std::optional<std::size_t> slidingWindow() const { return sliding_window_; }

public slots:

    /**
     * @brief Slot to notify of a new sender connection
     * @details Used to mark senders as available or absent
     *
     * @param sender Name of the sender
     */
    void senderConnected(const QString& sender) { metricsChanged(sender, {}); }

    /**
     * @brief Slot to notify of new metric topics being available
     * @details Used to mark metrics as available or absent
     *
     * @param sender Name of the sender
     * @param metrics List of available metrics from that sender
     */
    void metricsChanged(const QString& sender, const QStringList& metrics);

    /**
     * @brief Slot to notify of a  sender that disconnected
     * @details Used to mark senders as available or absent
     *
     * @param sender Name of the sender
     */
    void senderDisconnected(const QString& sender);

public slots:
    /**
     * @brief Public slot to update the metric
     * @details This slot is called from a signal emitted by the telemetry listener and adds new data points to the chart
     *
     * @param sender Name of the sender to select correct input
     * @param metric Name of the metric to filter correct metric
     * @param unit Unit of the metric
     * @param time Time value for x axis
     * @param value Variant holding the metric value for the y axis
     */
    void
    update(const QString& sender, const QString& metric, const QString& unit, const QDateTime& time, const QVariant& value);

signals:
    /**
     * @brief Signal to indicate a deletion request
     */
    void deleteRequested();

private:
    /**
     * @brief Helper tor rescale the x axis of the chart
     *
     * @param time New time to be added to the axis
     */
    void rescale_axes(const QDateTime& time);

    std::unique_ptr<QT_CHART QChartView> chart_view_;
    QMap<QString, QMetricSeries*> series_;

    // Axes
    QT_CHART QDateTimeAxis axis_x_;
    QT_CHART QValueAxis axis_y_;

    // Metric information
    QString metric_;
    Type type_;

    // Sliding window settings
    std::optional<std::size_t> sliding_window_;

    // Buttons & labels
    QVBoxLayout layout_;
    QHBoxLayout tool_bar_;
    QLineEdit title_label_;
    QToolButton pause_btn_;
    QToolButton reset_btn_;
    QToolButton delete_btn_;
    QColor bg_color_;
};
