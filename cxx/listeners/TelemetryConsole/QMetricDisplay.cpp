/**
 * @file
 * @brief Metric Chart Widget Implementation
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "QMetricDisplay.hpp"

#include <algorithm>

#include <QDateTime>
#include <QLabel>
#include <QToolButton>

#include <QtCharts/QChart>
#include <QtGui/QPainter>
#include <QtWidgets/QVBoxLayout>

QMetricDisplay::QMetricDisplay(
    const QString& sender, const QString& metric, bool sliding, std::size_t window, QWidget* parent)
    : QFrame(parent), chart_view_(std::make_unique<QChartView>(this)), window_sliding_(sliding), window_duration_(window),
      sender_(sender), metric_(metric) {

    // Set up axis labels and format
    axis_x_.setFormat("HH:mm:ss");
    axis_x_.setTitleText("Time");
    axis_y_.setTitleText(metric);

    auto* layout = new QVBoxLayout(this);
    auto* topBar = new QHBoxLayout();

    // Add title label above the metric display (optional)
    auto* titleLabel = new QLabel(sender + ": " + metric, this);
    titleLabel->setStyleSheet("font-weight: bold; margin-bottom: 4px;");

    auto* resetBtn = new QToolButton(this);
    resetBtn->setIcon(QIcon(":/action/reset"));
    resetBtn->setFixedSize(24, 24);
    resetBtn->setToolTip("Reset the data of this metric display");

    auto* deleteBtn = new QToolButton(this);
    deleteBtn->setIcon(QIcon(":/action/delete"));
    deleteBtn->setFixedSize(24, 24);
    deleteBtn->setToolTip("Delete this metric display");

    connect(resetBtn, &QToolButton::clicked, this, &QMetricDisplay::reset);
    connect(deleteBtn, &QToolButton::clicked, this, &QMetricDisplay::deleteRequested);

    topBar->addWidget(titleLabel);
    topBar->addStretch();
    topBar->addWidget(resetBtn);
    topBar->addWidget(deleteBtn);

    auto chart = chart_view_->chart();
    chart->addAxis(&axis_x_, Qt::AlignBottom);
    chart->addAxis(&axis_y_, Qt::AlignLeft);
    chart->legend()->hide();

    layout->addLayout(topBar);
    layout->addWidget(chart_view_.get());
    layout->setContentsMargins(6, 6, 6, 6);

    // Apply visual frame to thew widget
    setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    setLineWidth(1);
    setStyleSheet("QMetricDisplay { border: 1px solid #888; border-radius: 6px; background: #fafafa; }");
}

void QMetricDisplay::init_series(QXYSeries* series) {
    if(series == nullptr) {
        return;
    }

    if(series_) {
        series_->detachAxis(&axis_x_);
        series_->detachAxis(&axis_y_);
        chart_view_->chart()->removeSeries(series_);
    }

    series_ = series;
    chart_view_->chart()->addSeries(series_);
    series_->attachAxis(&axis_x_);
    series_->attachAxis(&axis_y_);
    reset();
}

void QMetricDisplay::reset() {
    if(series_) {
        series_->clear();
    }

    // Reset axis ranges
    axis_x_.setRange(QDateTime(), QDateTime());
    axis_y_.setRange(0.0, 1.0);
}

void QMetricDisplay::update(const QString& sender, const QString& metric, const QDateTime& x, const QVariant& y) {

    if(sender_ != sender || metric_ != metric) {
        return;
    }

    bool success = false;
    const double yd = y.toDouble(&success);
    if(!success) {
        return;
    }

    series_->append(x.toMSecsSinceEpoch(), yd);
    rescale_axes(x);
}

void QMetricDisplay::rescale_axes(const QDateTime& newTime) {

    // Rescale y axis according to min/max values
    const auto points = series_->points();
    const auto [min, max] = std::minmax_element(
        points.cbegin(), points.cend(), [](const QPointF& a, const QPointF& b) { return a.y() < b.y(); });

    if(min != points.cend() && max != points.cend()) {
        const auto span = std::max(1e-3, max->y() - min->y());
        axis_y_.setRange(min->y() - span * 0.05, max->y() + span * 0.05);
    }

    if(!axis_x_.min().isValid()) {
        axis_x_.setRange(QDateTime::currentDateTime(), newTime);
        return;
    }

    if(window_sliding_) {
        const auto end = newTime;
        const auto start = end.addSecs(-1 * static_cast<int>(window_duration_));
        axis_x_.setRange(start, end);
    } else {
        if(newTime > axis_x_.max())
            axis_x_.setMax(newTime);
    }
}
