/**
 * @file
 * @brief Helper classes for measurement conditions
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <string>

#include "constellation/build.hpp"
#include "constellation/controller/Controller.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/Logger.hpp"

namespace constellation::controller {

    /**
     * @brief Base class for measurement conditions
     */
    class CNSTLN_API MeasurementCondition {
    public:
        /**
         * @brief Method waiting for the condition to become true, blocking call
         * @details This is the purely virtual interface method to be implemented by condition classes
         *
         * @param running Reference to the running state of the measurement queue
         * @param controller Reference to the controller used by the measurement queue
         * @param logger Reference to the logger used by the measurement queue
         */
        virtual void await(std::atomic_bool& running, Controller& controller, log::Logger& logger) const = 0;

        /**
         * @brief Provide human-readable representation of the condition
         * @return Condition description
         */
        virtual std::string str() const = 0;

        virtual ~MeasurementCondition() = default;

        /// @cond doxygen_suppress
        // No copy/move constructor/assignment
        MeasurementCondition(const MeasurementCondition& other) = delete;
        MeasurementCondition& operator=(const MeasurementCondition& other) = delete;
        MeasurementCondition(MeasurementCondition&& other) = delete;
        MeasurementCondition& operator=(MeasurementCondition&& other) = delete;
        /// @endcond

    protected:
        MeasurementCondition() = default;
    };

    /**
     * @brief Class implementing a timer-based measurement condition
     */
    class CNSTLN_API TimerCondition : public MeasurementCondition {
    public:
        /**
         * @brief Constructor for a timer-based condition
         * @details: This condition will wait until the configured duration has passed
         *
         * @param duration Duration of this measurement
         */
        TimerCondition(std::chrono::seconds duration) : duration_(duration) {};

        void await(std::atomic_bool& running, Controller& controller, log::Logger& logger) const override;

        std::string str() const override;

    private:
        std::chrono::seconds duration_;
    };

    /**
     * @brief Class implementing a telemetry-based measurement condition
     */
    class CNSTLN_API MetricCondition : public MeasurementCondition {
    public:
        /**
         * @brief Constructor for a metric-based measurement condition
         * @details This condition will subscribe to the provided metric with the configured remote satellite and waits
         *          until the received value matches the target value and condition.
         *
         * @param remote Remote satellite to subscribe to
         * @param metric Metric to subscribe to from remote satellite
         * @param target Target value of the metric
         * @param comparator Comparison function the received metric value and the target value should satisfy
         * @param comp_name String representation of the comparator function, e.g. ">="

         */
        MetricCondition(std::string remote,
                        std::string metric,
                        config::Value target,
                        std::function<bool(const config::Value, const config::Value)> comparator = std::greater_equal<>(),
                        std::string comp_name = ">=");

        void await(std::atomic_bool& running, Controller& controller, log::Logger& logger) const override;

        std::string str() const override;

    private:
        std::string remote_;
        std::string metric_;
        config::Value target_;
        std::function<bool(config::Value, config::Value)> comparator_;
        std::string comparator_str_;
        std::chrono::seconds metric_reception_timeout_;
    };

} // namespace constellation::controller
