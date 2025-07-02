/**
 * @file
 * @brief Metric Series Implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QMetricSeries.hpp"

#include <QApplication>
#include <QDateTime>
#include <QGraphicsLayout>
#include <QGraphicsScene>
#include <QToolButton>

#include <QtCharts/QChart>
#include <QtGui/QPainter>
#include <QtWidgets/QVBoxLayout>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

QMetricSeries::QMetricSeries(QChart* chart) {
    chart->scene()->addItem(&value_marker_);
}

void QMetricSeries::updateMarker(QChart* chart, const QString& unit) {

    const auto points = this->points();

    // Update marker/label for last value
    if(points.isEmpty()) {
        return;
    }
    // Update value label
    const auto scene_pos = chart->mapToPosition(points.last(), series());

    // Shift position of last point by bounding box size to right-align
    value_marker_.setPlainText(QString::number(points.last().y(), 'f', 2) + (unit.isEmpty() ? "" : " " + unit));
    const auto bounds = value_marker_.boundingRect();
    value_marker_.setPos(scene_pos.x() - bounds.width(), scene_pos.y() - 10 - bounds.height());
}

QSplineMetricSeries::QSplineMetricSeries(QChart* chart) : QMetricSeries(chart), spline_(new QSplineSeries()) {
    chart->addSeries(spline_);
}

void QSplineMetricSeries::clear() {
    spline_->clear();
};

QList<QPointF> QSplineMetricSeries::points() const {
    return spline_->points();
};

void QSplineMetricSeries::append(qint64 x, double y) {
    spline_->append(static_cast<double>(x), y);
}

QScatterMetricSeries::QScatterMetricSeries(QChart* chart) : QMetricSeries(chart), scatter_(new QScatterSeries()) {
    scatter_->setMarkerSize(8.0);
    chart->addSeries(scatter_);
}

void QScatterMetricSeries::clear() {
    scatter_->clear();
};

QList<QPointF> QScatterMetricSeries::points() const {
    return scatter_->points();
};

void QScatterMetricSeries::append(qint64 x, double y) {
    scatter_->append(static_cast<double>(x), y);
}

QAreaMetricSeries::QAreaMetricSeries(QChart* chart)
    : QMetricSeries(chart), spline_(new QSplineSeries()), lower_(new QLineSeries()),
      area_series_(new QAreaSeries(spline_, lower_)) {
    chart->addSeries(area_series_);
}

void QAreaMetricSeries::clear() {
    spline_->clear();
    lower_->clear();
};

QList<QPointF> QAreaMetricSeries::points() const {
    return spline_->points();
};

void QAreaMetricSeries::append(qint64 x, double y) {
    spline_->append(static_cast<double>(x), y);
    lower_->append(static_cast<double>(x), 0);
}
