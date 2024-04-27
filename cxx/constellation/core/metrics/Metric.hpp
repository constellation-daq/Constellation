/**
 * @file
 * @brief Metrics
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <chrono>
#include <string_view>

#include "constellation/core/config/Dictionary.hpp"

namespace constellation::metrics {

    /** Metrics types */
    enum class Type : std::uint8_t {
        LAST_VALUE = 1,
        ACCUMULATE = 2,
        AVERAGE = 3,
        RATE = 4,
    };
    using enum Type;

    using Clock = std::chrono::high_resolution_clock;

    class Metric {
    public:
        Metric(std::string_view unit, const Type type, config::Value value)
            : unit_(unit), type_(type), value_(std::move(value)) {}

        virtual ~Metric() noexcept = default;

        // Default copy/move constructor/assignment
        Metric(const Metric&) = default;
        Metric& operator=(const Metric&) = default;
        Metric(Metric&&) noexcept = default;
        Metric& operator=(Metric&&) = default;

        void set(const config::Value& value) { value_ = value; }

        config::Value value() const { return value_; }
        std::string unit() const { return unit_; }
        Type type() const { return type_; }

    private:
        std::string unit_ {};
        Type type_;
        config::Value value_ {};
    };

    class MetricTimer : public Metric {
    public:
        MetricTimer(std::string_view unit, const Type type, config::Value value = {}) : Metric(unit, type, value) {}

        bool check();
        virtual Clock::time_point next_trigger() const { return Clock::time_point::max(); }

        virtual void update(const config::Value& value);

    protected:
        virtual bool condition() = 0;

    private:
        bool changed_ {true};
    };

    class TimedMetric : public MetricTimer {
    public:
        TimedMetric(std::string_view unit, Type type, Clock::duration interval, config::Value value = {})
            : MetricTimer(unit, type, std::move(value)), interval_(interval), last_trigger_(Clock::now()) {}

        bool condition() override;
        Clock::time_point next_trigger() const override;

    private:
        Clock::duration interval_;
        Clock::time_point last_trigger_;
    };

    class TriggeredMetric : public MetricTimer {
    public:
        TriggeredMetric(std::string_view unit, Type type, std::size_t triggers, const config::Value& value);

        void update(const config::Value& value) override;

        bool condition() override;

    private:
        std::size_t triggers_;
        std::size_t current_triggers_ {0};
    };
} // namespace constellation::metrics
