/**
 * @file
 * @brief Metric chart display Widget
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include <QList>
#include <QString>

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

class QMetricSeries {
public:
    explicit QMetricSeries(QChart* /*chart*/) {};
    virtual ~QMetricSeries() = default;

    virtual void clear() = 0;
    virtual QList<QPointF> points() const = 0;
    virtual void append(qint64 x, double y) = 0;
};

class QSplineMetricSeries : public QMetricSeries {
public:
    QSplineMetricSeries(QChart* chart);
    void clear() override;
    QList<QPointF> points() const override;
    void append(qint64 x, double y) override;

private:
    QT_CHART QSplineSeries* spline_;
};

class QScatterMetricSeries : public QMetricSeries {
public:
    QScatterMetricSeries(QChart* chart);
    void clear() override;
    QList<QPointF> points() const override;
    void append(qint64 x, double y) override;

private:
    QT_CHART QScatterSeries* scatter_;
};

class QAreaMetricSeries : public QMetricSeries {
public:
    QAreaMetricSeries(QChart* chart);
    void clear() override;
    QList<QPointF> points() const override;
    void append(qint64 x, double y) override;

private:
    QT_CHART QSplineSeries* spline_;
    QT_CHART QLineSeries* lower_;
    QT_CHART QAreaSeries* area_series_ = nullptr;
};
