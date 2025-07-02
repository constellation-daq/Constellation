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
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include <QApplication>
#include <QDateTime>
#include <QGraphicsLayout>
#include <QMap>
#include <QToolButton>

#include "constellation/gui/qt_utils.hpp"

#include "QMetricSeries.hpp"
#include <QtCharts/QChart>
#include <QtGui/QPainter>
#include <QtWidgets/QVBoxLayout>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

using namespace constellation::gui;

QMetricDisplay::QMetricDisplay(const QString& metric, Type type, std::optional<std::size_t> window, QWidget* parent)
    : QFrame(parent), chart_view_(std::make_unique<QChartView>(this)), metric_(metric), type_(type), sliding_window_(window),
      layout_(this), title_label_(metric, this) {

    // Set up axis labels and format
    axis_x_.setFormat("HH:mm:ss");
    axis_x_.setTitleText("Time");
    axis_y_.setTitleText(metric);

    title_label_.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    title_label_.setReadOnly(true);
    title_label_.setFrame(false);
    title_label_.setStyleSheet("background: transparent; border: none; font-weight: bold;");
    title_label_.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    pause_btn_.setIcon(QIcon(":/action/pause"));
    pause_btn_.setFixedSize(24, 24);
    pause_btn_.setToolTip("Pause this metric display");
    pause_btn_.setCheckable(true);

    reset_btn_.setIcon(QIcon(":/action/reset"));
    reset_btn_.setFixedSize(24, 24);
    reset_btn_.setToolTip("Reset the data of this metric display");

    delete_btn_.setIcon(QIcon(":/action/delete"));
    delete_btn_.setFixedSize(24, 24);
    delete_btn_.setToolTip("Delete this metric display");

    connect(&reset_btn_, &QToolButton::clicked, this, &QMetricDisplay::reset);
    connect(&delete_btn_, &QToolButton::clicked, this, &QMetricDisplay::deleteRequested);

    tool_bar_.addWidget(&title_label_);
    tool_bar_.addStretch();
    tool_bar_.addWidget(&pause_btn_);
    tool_bar_.addWidget(&reset_btn_);
    tool_bar_.addWidget(&delete_btn_);

    const auto current = this->palette().color(QPalette::Window);
    bg_color_ = is_dark_mode() ? current.darker(120) : current.lighter(120);

    auto* chart = chart_view_->chart();
    chart->addAxis(&axis_x_, Qt::AlignBottom);
    chart->addAxis(&axis_y_, Qt::AlignLeft);
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setMargins(QMargins(3, 3, 3, 3));
    chart->legend()->setAlignment(Qt::AlignTop);
    chart->setBackgroundVisible(false);
    chart->setTheme(is_dark_mode() ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    chart_view_->setBackgroundBrush(Qt::NoBrush);
    chart_view_->setStyleSheet("background: transparent");

    layout_.addLayout(&tool_bar_);
    layout_.addWidget(chart_view_.get());
    layout_.setContentsMargins(6, 6, 6, 6);

    // Set size policy
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Apply visual frame to the widget
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Plain);
    setLineWidth(1);
    setStyleSheet("QMetricDisplay { border: 1px solid gray; border-radius: 6px; " +
                  QString("background-color: %1;").arg(bg_color_.name()) + " }");

    // Reset axes
    reset();
}

void QMetricDisplay::addSender(const QString& sender) {

    // Don't add twice
    if(hasSender(sender)) {
        return;
    }

    auto* chart = chart_view_->chart();
    std::unique_ptr<QMetricSeries> series;

    switch(type_) {
    case Type::Spline: {
        series = std::make_unique<QSplineMetricSeries>(chart);
        break;
    }
    case Type::Scatter: {
        series = std::make_unique<QScatterMetricSeries>(chart);
        break;
    }
    case Type::Area: {
        series = std::make_unique<QAreaMetricSeries>(chart);
        break;
    }
    default: std::unreachable();
    }

    // Attach axes for new series
    auto* s = chart->series().last();
    s->attachAxis(&axis_x_);
    s->attachAxis(&axis_y_);
    s->setName(sender);

    // Add to list
    series_.insert(sender, series.release());
}

void QMetricDisplay::senderDisconnected(const QString& sender) {
    if(hasSender(sender)) {
        title_label_.setStyleSheet("background: transparent; border: none; font-weight: bold; color: red");
        setStyleSheet("QMetricDisplay { border: 1px solid gray; border-radius: 6px; " +
                      QString("background-color: %1;").arg(QColor(255, 0, 0, 24).name(QColor::HexArgb)) + " }");
    }
}

void QMetricDisplay::metricsChanged(const QString& sender, const QStringList& metrics) {
    if(!hasSender(sender)) {
        return;
    }

    if(!metrics.contains(metric_)) {
        title_label_.setStyleSheet("background: transparent; border: none; font-weight: bold; color: orange");
        setStyleSheet("QMetricDisplay { border: 1px solid gray; border-radius: 6px; " +
                      QString("background-color: %1;").arg(QColor(255, 138, 0, 32).name(QColor::HexArgb)) + " }");
    } else {
        title_label_.setStyleSheet("background: transparent; border: none; font-weight: bold;");
        setStyleSheet("QMetricDisplay { border: 1px solid gray; border-radius: 6px; " +
                      QString("background-color: %1;").arg(bg_color_.name()) + " }");
    }
}

void QMetricDisplay::reset() {
    // Clear all series
    for(auto& s : series_) {
        s->clear();
    }

    // Reset axis ranges
    axis_x_.setMin(QDateTime::currentDateTime().addSecs(-1));
    axis_x_.setMax(QDateTime::currentDateTime());
    axis_y_.setRange(0.0, 1.0);
}

void QMetricDisplay::update(
    const QString& sender, const QString& metric, const QString& unit, const QDateTime& time, const QVariant& value) {

    // Check metric
    if(metric_ != metric) {
        return;
    }

    // Get series for this sender
    auto* series = series_.value(sender, nullptr);
    if(series == nullptr) {
        return;
    }

    bool success = false;
    const double yd = value.toDouble(&success);
    if(!success) {
        return;
    }

    // Append point to series
    series->append(time.toMSecsSinceEpoch(), yd);

    // Rescale axes and update labels unless paused
    if(pause_btn_.isChecked()) {
        return;
    }

    rescale_axes(time);
    axis_y_.setTitleText(metric + (unit.isEmpty() ? "" : " [" + unit + "]"));
    series->updateMarker(chart_view_->chart(), unit);
}

void QMetricDisplay::rescale_axes(const QDateTime& newTime) {
    const auto local_time = newTime.toLocalTime();

    // Rescale y axis according to min/max values
    double global_min = std::numeric_limits<double>::max();
    double global_max = std::numeric_limits<double>::lowest();
    bool has_points = false;

    for(auto* series : series_) {
        const auto& points = series->points();
        if(points.isEmpty()) {
            continue;
        }

        has_points = true;
        for(const auto& point : points) {
            global_min = std::min(global_min, point.y());
            global_max = std::max(global_max, point.y());
        }
    }

    if(has_points) {
        const double span = std::max(1e-3, global_max - global_min);
        axis_y_.setRange(global_min - (span * 0.1), global_max + (span * 0.1));
    }

    if(sliding_window_.has_value()) {
        const auto start = local_time.addSecs(-1 * static_cast<qint64>(sliding_window_.value()));
        axis_x_.setRange(start, local_time);
        return;
    }

    if(local_time > axis_x_.max()) {
        axis_x_.setMax(local_time);
    }
    if(local_time < axis_x_.min()) {
        axis_x_.setMin(local_time);
    }
}
