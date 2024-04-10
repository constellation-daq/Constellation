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
        Metric(const Type type) : type_(type) {};

        Metric(const Type type, config::Value value) : type_(type), value_(std::move(value)), changed_(true) {}

        virtual ~Metric() noexcept = default;

        // Default copy/move constructor/assignment
        Metric(const Metric&) = default;
        Metric& operator=(const Metric&) = default;
        Metric(Metric&&) noexcept = default;
        Metric& operator=(Metric&&) = default;

        virtual void set(const config::Value& value);

        config::Value value() const { return value_; }

        Type type() const { return type_; }

        bool check();
        virtual Clock::time_point next_trigger() const { return Clock::time_point::max(); }

    protected:
        virtual bool condition() = 0;

    private:
        Type type_;
        config::Value value_ {};
        bool changed_ {false};
    };

    class TimedMetric : public Metric {
    public:
        TimedMetric(Clock::duration interval, Type type, config::Value value = {})
            : Metric(type, std::move(value)), interval_(interval), last_trigger_(Clock::now()) {}

        bool condition() override;
        Clock::time_point next_trigger() const override;

    private:
        Clock::duration interval_;
        Clock::time_point last_trigger_;
    };

    class TriggeredMetric : public Metric {
    public:
        TriggeredMetric(std::size_t triggers, Type type, config::Value value);

        void set(const config::Value& value) override;

        bool condition() override;

    private:
        std::size_t triggers_;
        std::size_t current_triggers_ {0};
    };
} // namespace constellation::metrics
