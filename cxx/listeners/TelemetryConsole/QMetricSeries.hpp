/**
 * @file
 * @brief Metric chart display Widget
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <QGraphicsTextItem>
#include <QList>
#include <QString>

#include <QtCharts/QAbstractSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QChart>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QXYSeries>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#define QT_CHART QtCharts::
#else
#define QT_CHART
#endif

/**
 * @class QMetricSeries
 * @brief Abstract base class for metric series to be displayed in a chart
 */
class QMetricSeries {
public:
    /**
     * @brief Constructor, registers value label with the chart
     *
     * @param chart Chart this series will be attached to
     */
    explicit QMetricSeries(QT_CHART QChart* chart);

    virtual ~QMetricSeries() = default;

    // No copy constructor/assignment/move constructor/assignment
    /// @cond doxygen_suppress
    QMetricSeries(const QMetricSeries& other) = delete;
    QMetricSeries& operator=(const QMetricSeries& other) = delete;
    QMetricSeries(QMetricSeries&& other) noexcept = delete;
    QMetricSeries& operator=(QMetricSeries&& other) = delete;
    /// @endcond

    /**
     * @brief Clear all data points
     * @details Purely virtual method to be implemented by concrete series
     */
    virtual void clear() = 0;

    /**
     * @brief Get list of all points of the series
     * @details Purely virtual method to be implemented by concrete series
     */
    virtual QList<QPointF> points() const = 0;

    /**
     * @brief Append a new data point to the series
     * @details Purely virtual method to be implemented by concrete series
     */
    virtual void append(qint64 x, double y) = 0;

    /**
     * @brief Update the value marker
     * @details Purely virtual method to be implemented by concrete series
     */
    void updateMarker(QT_CHART QChart* chart, const QString& unit);

protected:
    /**
     * @brief Helper to get the main series
     * @return Pointer to series with data points
     */
    virtual QT_CHART QAbstractSeries* series() const = 0;

private:
    QGraphicsTextItem* value_marker_;
};

class QSplineMetricSeries : public QMetricSeries {
public:
    QSplineMetricSeries(QT_CHART QChart* chart);
    void clear() override;
    QList<QPointF> points() const override;
    void append(qint64 x, double y) override;

private:
    QT_CHART QAbstractSeries* series() const override { return spline_; }
    QT_CHART QSplineSeries* spline_;
};

class QScatterMetricSeries : public QMetricSeries {
public:
    QScatterMetricSeries(QT_CHART QChart* chart);
    void clear() override;
    QList<QPointF> points() const override;
    void append(qint64 x, double y) override;

private:
    QT_CHART QAbstractSeries* series() const override { return scatter_; }
    QT_CHART QScatterSeries* scatter_;
};

class QAreaMetricSeries : public QMetricSeries {
public:
    QAreaMetricSeries(QT_CHART QChart* chart);
    void clear() override;
    QList<QPointF> points() const override;
    void append(qint64 x, double y) override;

private:
    QT_CHART QAbstractSeries* series() const override { return area_series_; }
    QT_CHART QSplineSeries* spline_;
    QT_CHART QLineSeries* lower_;
    QT_CHART QAreaSeries* area_series_;
};
