/**
 * @file
 * @brief Qt utilities
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <utility>

#include <QColor>
#include <QDateTime>
#include <QGuiApplication>
#include <QString>
#include <QStyleHints>
#include <QtGlobal>
#include <QTimeZone>

#include "constellation/core/log/Level.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/utils/enum.hpp"

#if __cpp_lib_format >= 201907L
#include <format>
#endif

namespace constellation::gui {

    /** Check if in dark mode (requires Qt 6.5 or newer) */
    inline bool is_dark_mode() {
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
        return false;
#else
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#endif
    }

    /**
     * @brief Helper to obtain the CSCP message type string with color and formatting
     *
     * @param type CSCP message type
     *
     * @return String for the CSCP response display
     */
    inline QString get_styled_response(message::CSCP1Message::Type type) {
        const auto type_string = QString::fromStdString(utils::enum_name(type));
        switch(type) {
        case message::CSCP1Message::Type::REQUEST:
        case message::CSCP1Message::Type::NOTIMPLEMENTED: {
            return "<font color='gray'>" + type_string + "</font>";
        }
        case message::CSCP1Message::Type::SUCCESS: {
            return "<font color='green'>" + type_string + "</font>";
        }
        case message::CSCP1Message::Type::INCOMPLETE:
        case message::CSCP1Message::Type::INVALID:
        case message::CSCP1Message::Type::UNKNOWN: {
            return "<font color='orange'>" + type_string + "</font>";
        }
        case message::CSCP1Message::Type::ERROR: {
            return "<font color='darkred'>" + type_string + "</font>";
        }
        default: std::unreachable();
        }
    }

    /** Color assignment for log levels */
    inline QColor get_log_level_color(constellation::log::Level level) {
        switch(level) {
        case constellation::log::Level::TRACE: {
            if(is_dark_mode()) {
                return {67, 67, 67, 128};
            } else {
                return {224, 224, 224, 128};
            }
        }
        case constellation::log::Level::DEBUG: {
            if(is_dark_mode()) {
                return {85, 85, 85, 128};
            } else {
                return {200, 200, 200, 128};
            }
        }
        case constellation::log::Level::INFO: {
            if(is_dark_mode()) {
                return {100, 100, 100, 128};
            } else {
                return {191, 191, 191, 128};
            }
        }
        case constellation::log::Level::WARNING: {
            return {255, 138, 0, 128};
        }
        case constellation::log::Level::STATUS: {
            return {0, 100, 0, 128};
        }
        case constellation::log::Level::CRITICAL: {
            return {255, 0, 0, 128};
        }
        case constellation::log::Level::OFF: {
            return {0, 0, 0, 128};
        }
        default: std::unreachable();
        }
    };

    inline QDateTime from_timepoint(const std::chrono::system_clock::time_point& time_point) {
#if __cpp_lib_chrono >= 201907L
        return QDateTime::fromStdTimePoint(std::chrono::time_point_cast<std::chrono::milliseconds>(time_point));
#else
        return QDateTime(QDate(1970, 1, 1), QTime(0, 0, 0), QTimeZone::utc())
            .addMSecs(std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()).count());
#endif
    }

    inline QString duration_string(std::chrono::seconds duration) {
#if __cpp_lib_format >= 201907L
        return QString::fromStdString(std::format("{:%H:%M:%S}", duration));
#else
        const std::chrono::hh_mm_ss<std::chrono::seconds> fdur {duration};
        return QString("%1:%2:%3")
            .arg(fdur.hours().count(), 2, 10, QChar('0'))
            .arg(fdur.minutes().count(), 2, 10, QChar('0'))
            .arg(fdur.seconds().count(), 2, 10, QChar('0'));
#endif
    }
} // namespace constellation::gui
