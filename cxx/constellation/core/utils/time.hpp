/**
 * @file
 * @brief Time utilities
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

#if __cpp_lib_format >= 201907L
#include <format>
#endif

namespace constellation::utils {

    /**
     * @brief Convert a time in the local time zone into a system time point
     * @details This helper takes a number of hours, minutes and seconds of the day and converts them from the local time
     *          zone into UTC, and uses the current date to form a system-clock time point.
     *
     * @param hour Hour of the day
     * @param minute Minute of the day
     * @param second Second of the day
     * @return Time point in UTC using today as the date portion
     */
    inline std::chrono::system_clock::time_point localtime_to_system(std::uint8_t hour,
                                                                     std::uint8_t minute,
                                                                     std::uint8_t second) {

        const auto time_now = std::chrono::system_clock::now();

#if __cpp_lib_chrono >= 201907L
        const auto local_time_now = std::chrono::current_zone()->to_local(time_now);
        // Use midnight today plus local time from TOML
        const auto local_time = std::chrono::floor<std::chrono::days>(local_time_now) + std::chrono::hours {hour} +
                                std::chrono::minutes {minute} + std::chrono::seconds {second};
        const auto system_time = std::chrono::current_zone()->to_sys(local_time);
#else
        const auto time_t = std::chrono::system_clock::to_time_t(time_now);
        std::tm tm {};
        localtime_r(&time_t, &tm); // there is no thread-safe std::localtime
        // Use mightnight today plus local time from TOML
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        const auto local_time_t = std::mktime(&tm);
        const auto system_time = std::chrono::system_clock::from_time_t(local_time_t);
#endif

        return system_time;
    }

    /**
     * @brief Convert a local date into a system time point
     * @details This helper takes a year, month and day and forms a system time point from it, using midnight as the time
     *
     * @param year Year
     * @param month Month
     * @param day Day
     * @return Time point in UTC using midnight as the time portion
     */
    inline std::chrono::system_clock::time_point localdate_to_system(std::uint16_t year,
                                                                     std::uint8_t month,
                                                                     std::uint8_t day) {

        const auto ymd =
            std::chrono::year_month_day(std::chrono::year {year}, std::chrono::month {month}, std::chrono::day {day});

        // Convert to sys_days, a time_point with days precision since epoch
        const auto system_days = std::chrono::sys_days(ymd);
        return std::chrono::time_point_cast<std::chrono::system_clock::duration>(system_days);
    }

    /**
     * @brief Convert a system time point to a date and a time in the current time zone
     * @details This helper takes a time point in UTC and converts it into a date and a time in the local time zone
     *
     * @param time_point Time point in UTC
     * @return Date and time in local time zone
     */
    inline std::pair<std::chrono::year_month_day, std::chrono::hh_mm_ss<std::chrono::seconds>>
    system_to_localdatetime(const std::chrono::system_clock::time_point time_point) {
        std::pair<std::chrono::year_month_day, std::chrono::hh_mm_ss<std::chrono::seconds>> datetime;

#if __cpp_lib_chrono >= 201907L
        const auto local_time = std::chrono::zoned_time(std::chrono::current_zone(), time_point);
        auto local_timepoint = local_time.get_local_time();
        auto date = std::chrono::floor<std::chrono::days>(local_timepoint);

        datetime.first = std::chrono::year_month_day {date};
        datetime.second = std::chrono::hh_mm_ss<std::chrono::seconds> {
            std::chrono::duration_cast<std::chrono::seconds>(local_timepoint - date)};
#else
        const auto time_t = std::chrono::system_clock::to_time_t(time_point);
        std::tm tm {};
        localtime_r(&time_t, &tm); // there is no thread-safe std::localtime
        datetime.first = std::chrono::year_month_day {std::chrono::year {tm.tm_year + 1900},
                                                      std::chrono::month {static_cast<unsigned int>(tm.tm_mon + 1)},
                                                      std::chrono::day {static_cast<unsigned int>(tm.tm_mday)}};
        datetime.second = std::chrono::hh_mm_ss<std::chrono::seconds> {
            std::chrono::hours {tm.tm_hour} + std::chrono::minutes {tm.tm_min} + std::chrono::seconds {tm.tm_sec}};
#endif
        return datetime;
    }

    /** Converts an std::chrono::system_clock::time_point to a RFC3339 string in local time */
    inline std::string to_rfc3339_string(std::chrono::system_clock::time_point tp) {
#if defined __cpp_lib_format && __cpp_lib_chrono >= 201907L
        const auto local_time = std::chrono::zoned_time(std::chrono::current_zone(), tp);
        return std::format("{0:%F}T{0:%T}", local_time);
#else
        // Convert time point to tm struct
        const auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm {};
        localtime_r(&time_t, &tm); // there is no thread-safe std::gmtime
        // Format tm as YYYY-MM-DD HH:MM:SS
        std::ostringstream oss {};
        oss << std::put_time(&tm, "%FT%T");
        // Get nanoseconds since the last second
        const auto tp_in_s = std::chrono::time_point_cast<std::chrono::seconds>(tp);
        const auto ns_diff = std::chrono::time_point_cast<std::chrono::nanoseconds>(tp) -
                             std::chrono::time_point_cast<std::chrono::nanoseconds>(tp_in_s);
        oss << "." << std::setw(9) << std::setfill('0') << ns_diff.count();
        return oss.str();
#endif
    }

    inline std::chrono::system_clock::time_point datetime_to_system(std::uint16_t year,
                                                                    std::uint8_t month,
                                                                    std::uint8_t day,
                                                                    std::uint8_t hour,
                                                                    std::uint8_t minute,
                                                                    std::uint8_t second,
                                                                    const std::string& tz) {

        // Create date
        const auto ymd =
            std::chrono::year_month_day {std::chrono::year {year}, std::chrono::month {month}, std::chrono::day {day}};
        const auto date = std::chrono::sys_days {ymd};
        const auto time = std::chrono::hours {hour} + std::chrono::minutes {minute} + std::chrono::seconds {second};

        if(tz == "Z") {
            return std::chrono::time_point_cast<std::chrono::system_clock::duration>(date + time);
        }

        if(tz.size() == 6) {
            // Subtract offset from local time to get UTC
            const auto sign = (tz.starts_with('-') ? -1 : 1);

            // Calculate offset from hours and minutes section
            const auto offset = sign * ((std::stoi(tz.substr(1, 2)) * 60) + std::stoi(tz.substr(4, 2)));
            return std::chrono::time_point_cast<std::chrono::system_clock::duration>(date + time -
                                                                                     std::chrono::minutes {offset});
        }

        // No offset given, interpreting as local time and converting to UTC
#if __cpp_lib_chrono >= 201907L
        const auto ldate = std::chrono::local_days {ymd};
        return std::chrono::current_zone()->to_sys(std::chrono::local_time<std::chrono::seconds>(ldate + time));
#else
        std::tm tm {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = -1;

        const std::time_t tt = std::mktime(&tm);
        return std::chrono::system_clock::from_time_t(tt);
#endif
    }
} // namespace constellation::utils
