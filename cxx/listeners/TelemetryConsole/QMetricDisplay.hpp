/**
 * @file
 * @brief Metric chart display Widget
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <memory>

#include <QDateTime>
#include <QFrame>
#include <QLabel>
#include <QString>
#include <QVariant>
#include <QWidget>

#include <QtCharts/QAbstractSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QXYSeries>

/**
 * @class QMetricDisplay
 * @brief Base class for displaying an auto-updating chart view of a given metric
 * @details The metric values are added via the update slot, the widget provides button for requesting its deletion as well
 * as for resetting of the data
 */
class QMetricDisplay : public QFrame {
    Q_OBJECT
public:
    /**
     * @brief Constructor of QMetricDisplay
     *
     * @param sender Name of the metric sender
     * @param metric Name of the metric
     * @param sliding Boolean to select sliding window or absolute time
     * @param window Window size for sliding window display
     * @param parent Parent QWidget
     */
    QMetricDisplay(
        const QString& sender, const QString& metric, bool sliding, std::size_t window, QWidget* parent = nullptr);

    /**
     * @brief Reset method
     * @details Clears all data of the time series
     */
    void reset();

    /**
     * @brief Get sender registered for this widget
     * @return Sender name
     */
    QString getSender() const { return sender_; }

    /**
     * @brief Get metric registered for this widget
     * @return Name of the metric
     */
    QString getMetric() const { return metric_; }

signals:
    /**
     * @brief Signal to indicate a deletion request
     */
    void deleteRequested();

public slots:
    /**
     * @brief Public slot to update the metric
     * @details This slot is called from a signal emitted by the telemetry listener and adds new data points to the chart
     *
     * @param sender Name of the sender to select correct input
     * @param metric Name of the metric to filter correct metric
     * @param x Time value for x axis
     * @param y Variant holding the metric value for the y axis
     */
    void update(const QString& sender, const QString& metric, const QDateTime& x, const QVariant& y);

protected:
    /**
     * @brief Helper to initialize data series from derived classes
     *
     * @param series Series holding the data for this chart
     */
    void init_series(QAbstractSeries* series);

    virtual void append_point(qint64 x, double y) = 0;

    virtual void clear() = 0;
    virtual QList<QPointF> points() = 0;

private:
    /**
     * @brief Helper tor rescale the x axis of the chart
     *
     * @param time New time to be added to the axis
     */
    void rescale_axes(const QDateTime& time);

    std::unique_ptr<QChartView> chart_view_;
    QAbstractSeries* series_ {nullptr};
    QLabel value_label_;

    // Axes
    QDateTimeAxis axis_x_;
    QValueAxis axis_y_;

    // Sliding window settings
    bool window_sliding_;
    std::size_t window_duration_;

    // Sender and metric information
    QString sender_;
    QString metric_;
};

class QSplineMetricDisplay : public QMetricDisplay {
    Q_OBJECT
public:
    QSplineMetricDisplay(
        const QString& sender, const QString& metric, bool sliding, std::size_t window, QWidget* parent = nullptr);

private:
    void clear() override;
    QList<QPointF> points() override;
    void append_point(qint64 x, double y) override;
    QSplineSeries* spline_;
};

class QScatterMetricDisplay : public QMetricDisplay {
    Q_OBJECT
public:
    QScatterMetricDisplay(
        const QString& sender, const QString& metric, bool sliding, std::size_t window, QWidget* parent = nullptr);

private:
    void clear() override;
    QList<QPointF> points() override;
    void append_point(qint64 x, double y) override;
    QScatterSeries* scatter_;
};

class QAreaMetricDisplay : public QMetricDisplay {
    Q_OBJECT
public:
    QAreaMetricDisplay(
        const QString& sender, const QString& metric, bool sliding, std::size_t window, QWidget* parent = nullptr);

private:
    void clear() override;
    QList<QPointF> points() override;
    void append_point(qint64 x, double y) override;
    QSplineSeries* spline_;
    QLineSeries* lower_;
    QAreaSeries* area_series_ = nullptr;
};
