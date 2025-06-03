/**
 * @file
 * @brief Helper class for queuing measurement series
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <stop_token>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>

#include "constellation/build.hpp"
#include "constellation/controller/Controller.hpp"
#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CSCP1Message.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"

namespace constellation::controller {

    /** Measurement queue class which allows to queue and fetch measurement configurations which can be used to reconfigure
     * a constellation
     */
    class CNSTLN_API MeasurementQueue {
    public:
        /** Measurement is a map with satellite canonical names as keys and configuration dictionaries as values */
        using Measurement = std::map<std::string, Controller::CommandPayload>;

        /**
         * @brief Base class for measurement conditions
         */
        class Condition {
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

        protected:
            Condition() = default;
        };

        /**
         * @brief Class implementing a timer-based measurement condition
         */
        class TimerCondition : public Condition {
        public:
            /**
             * @brief Constructor for a timer-based condition
             * @details: This condition will wait until the configured duration has passed
             *
             * @param duration Duration of this measurement
             */
            TimerCondition(std::chrono::seconds duration) : duration_(duration) {};

            void await(std::atomic_bool& running, Controller& controller, log::Logger& logger) const override;

        private:
            std::chrono::seconds duration_;
        };

        /**
         * @brief Class implementing a telemetry-based measurement condition
         */
        class MetricCondition : public Condition {
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

             */
            MetricCondition(std::string remote,
                            std::string metric,
                            config::Value target,
                            std::function<bool(config::Value, config::Value)> comparator);

            void await(std::atomic_bool& running, Controller& controller, log::Logger& logger) const override;

        private:
            std::string remote_;
            std::string metric_;
            config::Value target_;
            std::function<bool(config::Value, config::Value)> comparator_;
            std::chrono::seconds metric_reception_timeout_;
        };

        /**
         * @brief Construct a measurement queue
         *
         * @param controller Reference to the controller object to be used
         * @param prefix Prefix for the run identifier
         * @param condition Stopping condition for run stop in case no per-measurement condition is defined
         * @param timeout Transition timeout after which the queue will be interrupted if the target state was not reached
         */
        MeasurementQueue(Controller& controller,
                         std::string prefix,
                         std::shared_ptr<Condition> condition,
                         std::chrono::seconds timeout = std::chrono::seconds(60));
        /**
         * @brief Destruct the measurement queue
         */
        virtual ~MeasurementQueue();

        /// @cond doxygen_suppress
        // No copy/move constructor/assignment
        MeasurementQueue(const MeasurementQueue& other) = delete;
        MeasurementQueue& operator=(const MeasurementQueue& other) = delete;
        MeasurementQueue(MeasurementQueue&& other) = delete;
        MeasurementQueue& operator=(MeasurementQueue&& other) = delete;
        /// @endcond

        /**
         * @brief Append a new measurement
         *
         * @param measurement Measurement to be added to the queue
         * @param condition Optional condition for this specific measurement. If not provided, the queue's default condition
         *                  is used.
         */
        void append(Measurement measurement, std::shared_ptr<Condition> condition = nullptr);

        /**
         * @brief Helper to check if the queue is running
         * @return True if running, false otherwise
         */
        bool running() const { return queue_running_.load(); }

        /**
         * @brief Get number of remaining measurements
         * @return Remaining measurement queue length
         */
        std::size_t size() { return measurements_size_.load(); }

        /**
         * @brief Current progress of the measurement queue
         *
         * @details Returns the fraction of measurements successfully performed over the total number of measurements. Since
         * measurements are popped from the queue once they have been done, this uses the run sequence counter to account for
         * the number of measurements already performed.
         *
         * @return Progress of the queue, value as 0 < progress < 1
         */
        double progress() const;

        /**
         * @brief Start the measurement queue
         *
         * @note Requires the constellation to be in global state ORBIT
         */
        void start();

        /**
         * @brief Halt the measurement queue after the current measurement has concluded
         */
        void halt();

        /**
         * @brief Interrupt the current measurement and halt the queue
         */
        void interrupt();

    protected:
        /**
         * @brief Method called whenever a the queue started processing measurements
         */
        virtual void queue_started();

        /**
         * @brief Method called whenever a the queue stopped processing measurements
         */
        virtual void queue_stopped();

        /**
         * @brief Method called whenever a the queue failed or was interrupted
         */
        virtual void queue_failed();

        /**
         * @brief Method called whenever the progress of the queue was updated
         *
         * @param progress Current measurement progress of the queue, value as 0 < progress < 1
         */
        virtual void progress_updated(double progress);

    private:
        /** Queue loop, iterates over all measurements */
        void queue_loop(const std::stop_token& stop_token);

        /**
         * @brief Helper to wait for a global state in the constellation. Times out after the configured transition_timeout_
         *
         * @param state State to wait for
         */
        void await_state(protocol::CSCP::State state) const;

        /**
         * @brief Checks all satellite replies for success verbs.
         * @throws If a satellite did not respond with success
         *
         * @param replies Map of replies from the satellites
         */
        void check_replies(const std::map<std::string, message::CSCP1Message>& replies) const;

        /**
         * @brief Cache the original values of configuration parameters before performing a reconfiguration
         * @details Before each measurement, this method reads the configuration of the affected satellites, checks if the
         *          measurement key is present there and stores this value to reset it later. If the key is not present in
         *          the satellite configuration, or if the key has already been read previously, no value is cached.
         *
         * \param measurement Current measurement
         */
        void cache_original_values(const Measurement& measurement);

    private:
        /** Logger to use */
        log::Logger logger_;

        std::string run_identifier_prefix_;
        std::shared_ptr<Condition> default_condition_;
        std::chrono::seconds transition_timeout_;

        /** Queue of measurements */
        std::queue<std::pair<Measurement, std::shared_ptr<Condition>>> measurements_;
        std::mutex measurement_mutex_;
        std::atomic<std::size_t> measurements_size_ {0};
        std::atomic<std::size_t> run_sequence_ {0};

        /** Original parameters to be reset after the queue */
        Measurement original_values_;

        /** Interrupt counter to append to run identifier for re-tries */
        std::atomic<std::size_t> interrupt_counter_ {0};

        /** Controller to use for the measurements */
        Controller& controller_;

        /** Queue thread */
        std::jthread queue_thread_;
        std::atomic_bool queue_running_ {false};
    };

} // namespace constellation::controller
