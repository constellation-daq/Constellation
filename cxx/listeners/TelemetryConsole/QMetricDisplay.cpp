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
#include <memory>
#include <optional>

#include <QApplication>
#include <QDateTime>
#include <QGraphicsLayout>
#include <QToolButton>

#include "constellation/gui/qt_utils.hpp"

#include <QtCharts/QChart>
#include <QtGui/QPainter>
#include <QtWidgets/QVBoxLayout>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

using namespace constellation::gui;

QMetricDisplay::QMetricDisplay(
    const QString& sender, const QString& metric, bool sliding, std::size_t window, QWidget* parent)
    : QFrame(parent), chart_view_(std::make_unique<QChartView>(this)), window_sliding_(sliding), window_duration_(window),
      sender_(sender), metric_(metric), layout_(this), title_label_(metric, this) {

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
    chart->scene()->addItem(&value_marker_);
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

std::optional<std::size_t> QMetricDisplay::slidingWindow() const {
    if(window_sliding_) {
        return window_duration_;
    }
    return {};
}

void QMetricDisplay::setConnection(bool connected, bool metric) {

    if(!connected) {
        title_label_.setStyleSheet("background: transparent; border: none; font-weight: bold; color: red");
        setStyleSheet("QMetricDisplay { border: 1px solid gray; border-radius: 6px; " +
                      QString("background-color: %1;").arg(QColor(255, 0, 0, 24).name(QColor::HexArgb)) + " }");

    } else if(!metric) {
        title_label_.setStyleSheet("background: transparent; border: none; font-weight: bold; color: orange");
        setStyleSheet("QMetricDisplay { border: 1px solid gray; border-radius: 6px; " +
                      QString("background-color: %1;").arg(QColor(255, 138, 0, 32).name(QColor::HexArgb)) + " }");
    } else {
        title_label_.setStyleSheet("background: transparent; border: none; font-weight: bold;");
        setStyleSheet("QMetricDisplay { border: 1px solid gray; border-radius: 6px; " +
                      QString("background-color: %1;").arg(bg_color_.name()) + " }");
    }
}

void QMetricDisplay::init_series(QAbstractSeries* series) {
    if(series == nullptr) {
        return;
    }

    if(series_ != nullptr) {
        series_->detachAxis(&axis_x_);
        series_->detachAxis(&axis_y_);
        chart_view_->chart()->removeSeries(series_);
    }

    series_ = series;
    chart_view_->chart()->addSeries(series_);
    series_->attachAxis(&axis_x_);
    series_->attachAxis(&axis_y_);
    series_->setName(sender_);
    reset();
}

void QMetricDisplay::reset() {
    if(series_ != nullptr) {
        this->clear();
    }

    // Reset axis ranges
    axis_x_.setMin(QDateTime::currentDateTime().addSecs(-1));
    axis_x_.setMax(QDateTime::currentDateTime());
    axis_y_.setRange(0.0, 1.0);
}

void QMetricDisplay::update(
    const QString& sender, const QString& metric, const QString& unit, const QDateTime& time, const QVariant& value) {

    if(sender_ != sender || metric_ != metric) {
        return;
    }

    bool success = false;
    const double yd = value.toDouble(&success);
    if(!success) {
        return;
    }

    // Append point to series
    append_point(time.toMSecsSinceEpoch(), yd);

    // Rescale axes and update labels unless paused
    if(pause_btn_.isChecked()) {
        return;
    }

    rescale_axes(time);
    axis_y_.setTitleText(metric + (unit.isEmpty() ? "" : " [" + unit + "]"));

    // Update marker/label for last value
    if(this->points().isEmpty()) {
        return;
    }

    // Update value label
    const auto scene_pos = chart_view_->chart()->mapToPosition(this->points().last(), series_);

    // Shift position of last point by bounding box size to right-align
    value_marker_.setPlainText(QString::number(yd, 'f', 2) + (unit.isEmpty() ? "" : " " + unit));
    const auto bounds = value_marker_.boundingRect();
    value_marker_.setPos(scene_pos.x() - bounds.width(), scene_pos.y() - 10 - bounds.height());
}

void QMetricDisplay::rescale_axes(const QDateTime& newTime) {
    const auto local_time = newTime.toLocalTime();

    // Rescale y axis according to min/max values
    const auto& points = this->points();
    const auto& [min, max] =
        std::ranges::minmax_element(points, [](const QPointF& a, const QPointF& b) { return a.y() < b.y(); });

    if(min != points.cend() && max != points.cend()) {
        const auto span = std::max(1e-3, max->y() - min->y());
        axis_y_.setRange(min->y() - (span * 0.1), max->y() + (span * 0.1));
    }

    if(window_sliding_) {
        const auto start = local_time.addSecs(-1 * static_cast<qint64>(window_duration_));
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

QSplineMetricDisplay::QSplineMetricDisplay(
    const QString& sender, const QString& metric, bool sliding, std::size_t window, QWidget* parent)
    : QMetricDisplay(sender, metric, sliding, window, parent), spline_(new QSplineSeries()) {
    init_series(spline_);
}

void QSplineMetricDisplay::clear() {
    spline_->clear();
};

QList<QPointF> QSplineMetricDisplay::points() {
    return spline_->points();
};

void QSplineMetricDisplay::append_point(qint64 x, double y) {
    spline_->append(static_cast<double>(x), y);
}

QScatterMetricDisplay::QScatterMetricDisplay(
    const QString& sender, const QString& metric, bool sliding, std::size_t window, QWidget* parent)
    : QMetricDisplay(sender, metric, sliding, window, parent), scatter_(new QScatterSeries()) {
    scatter_->setMarkerSize(8.0);
    init_series(scatter_);
}

void QScatterMetricDisplay::clear() {
    scatter_->clear();
};

QList<QPointF> QScatterMetricDisplay::points() {
    return scatter_->points();
};

void QScatterMetricDisplay::append_point(qint64 x, double y) {
    scatter_->append(static_cast<double>(x), y);
}

QAreaMetricDisplay::QAreaMetricDisplay(
    const QString& sender, const QString& metric, bool sliding, std::size_t window, QWidget* parent)
    : QMetricDisplay(sender, metric, sliding, window, parent), spline_(new QSplineSeries()), lower_(new QLineSeries()),
      area_series_(new QAreaSeries(spline_, lower_)) {
    init_series(area_series_);
}

void QAreaMetricDisplay::clear() {
    spline_->clear();
    lower_->clear();
};

QList<QPointF> QAreaMetricDisplay::points() {
    return spline_->points();
};

void QAreaMetricDisplay::append_point(qint64 x, double y) {
    spline_->append(static_cast<double>(x), y);
    lower_->append(static_cast<double>(x), 0);
}
