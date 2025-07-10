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
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "constellation/build.hpp"
#include "constellation/controller/Controller.hpp"
#include "constellation/controller/MeasurementCondition.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/core/message/CSCP1Message.hpp"

namespace constellation::controller {

    /**
     * @brief Measurement queue class which allows to queue and fetch measurement configurations which can be used to
     * reconfigure a constellation
     *
     * The measurement queue holds a reference to the currently used controller of the constellation and can take over when
     * the global state is ORBIT, i.e. all satellites have been initialized and launched, It will only take care of
     * reconfiguring, starting and stopping, and will leave the constellation in the ORBIT state when finishing.
     *
     * Each measurement consists of a set parameters for any number of satellites. The original values of the measurement
     * parameters are read from the satellites using the `get_config` command before each measurement and are cached in the
     * queue. WHenever a parameter does not appear in the measurement anymore, it is reset to the original value the next
     * time a reconfiguration is performed.
     *
     * For example, a queue that first scans the parameter `a` with values from `1 - 2` and the parameter `b` from `5 - 7`
     * will first read the parameter `a` from the satellite configuration, then set `a = 1` and `a = 2` in the subsequent
     * measurement. The next measurement will set `b = 5`, but parameter `a` does not appear anymore and is reset to its
     * original value read from the satellite.
     */
    class CNSTLN_API MeasurementQueue {
    public:
        /** Measurement is a map with satellite canonical names as keys and configuration dictionaries as values */
        using Measurement = std::map<std::string, Controller::CommandPayload>;

        enum class State : std::uint8_t {
            IDLE,     ///< Queue is idling (there are pending measurements but the queue is stopped)
            FINISHED, ///< Queue is finished (there are no measurements in the queue and it is stopped)
            RUNNING,  ///< Queue is currently running
            FAILED,   ///< Queue has experienced a failure and has stopped
        };

        /**
         * @brief Construct a measurement queue
         *
         * @param controller Reference to the controller object to be used
         * @param timeout Transition timeout after which the queue will be interrupted if the target state was not reached
         */
        MeasurementQueue(Controller& controller, std::chrono::seconds timeout = std::chrono::seconds(60));
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
         * @brief Set run identifier prefix
         *
         * @param prefix Prefix for the run identifier
         */
        void setPrefix(std::string prefix);

        /**
         * @brief Set default condition
         *
         * @param condition Stopping condition for run stop in case no per-measurement condition is defined
         */
        void setDefaultCondition(std::shared_ptr<MeasurementCondition> condition);

        /**
         * @brief Append a new measurement.
         * @details Appends the measurement to the queue and updates the progress. The currently configured default condition
         *          is set for this measurement unless a measurement-specific condition is provided.
         *
         * @param measurement Measurement to be added to the queue
         * @param condition Optional condition for this specific measurement. If not provided, the queue's default condition
         *                  is used.
         */
        void append(Measurement measurement, std::shared_ptr<MeasurementCondition> condition = nullptr);

        /**
         * @brief Clear all measurements
         * @details If the queue is not running, this will clear all measurements. If the queue is currently running, it will
         *          clear all but the current measurement
         */
        void clear();

        /**
         * @brief Helper to check if the queue is running
         * @return True if running, false otherwise
         */
        bool running() const { return queue_running_.load(); }

        /**
         * @brief Get number of remaining measurements
         * @return Remaining measurement queue length
         */
        std::size_t size() const { return measurements_size_.load(); }

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
         * @brief Method called whenever a the queue state changed, e.g. it started or stopped processing measurements, or
         *        the queue failed
         *
         * @param queue_state New state of the queue
         * @param reason Reason for the state change, may be empty
         */
        virtual void queue_state_changed(State queue_state, std::string_view reason);

        /**
         * @brief Method called whenever a measurement was successfully concluded and has been removed from the queue
         */
        virtual void measurement_concluded();

        /**
         * @brief Method called whenever the progress of the queue was updated
         *
         * @param current Current number of measurements already successfully finished
         * @param total Total number of measurements (finished and pending) of this queue
         */
        virtual void progress_updated(std::size_t current, std::size_t total);

    private:
        /** Queue loop, iterates over all measurements */
        void queue_loop(const std::stop_token& stop_token);

        /**
         * @brief Get map of satellites and the timestamp of their last state change
         *
         * @param measurement Measurement from which to extract to extract relevant satellites
         * @return Map of satellites names and the timestamp of their last state change
         */
        std::map<std::string, std::chrono::system_clock::time_point>
        get_last_state_change(const Measurement& measurement) const;

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
         *          The actual measurement dictionary is amended with keys from the original values cache whenever this value
         *          does not appear anymore.
         *
         * @param measurement Current measurement
         */
        void cache_original_values(Measurement& measurement);

        /**
         * @brief Load current measurement number and total number of measurements
         *
         * @return Pair with current measurement number and total number of measurements
         */
        std::pair<std::size_t, std::size_t> load_progress() const;

    protected:
        /** Queue of measurements */
        // NOLINTBEGIN(*-non-private-member-variables-in-classes)
        std::deque<std::pair<Measurement, std::shared_ptr<MeasurementCondition>>> measurements_;
        mutable std::mutex measurement_mutex_;
        std::shared_ptr<MeasurementCondition> default_condition_;
        // NOLINTEND(*-non-private-member-variables-in-classes)

    private:
        /** Logger to use */
        log::Logger logger_;

        std::string run_identifier_prefix_;
        std::chrono::seconds transition_timeout_;

        std::atomic_size_t measurements_size_ {0};
        std::atomic_size_t run_sequence_ {0};

        /** Original parameters to be reset after the queue */
        Measurement original_values_;

        /** Interrupt counter to append to run identifier for re-tries */
        std::atomic_size_t interrupt_counter_ {0};

        /** Controller to use for the measurements */
        Controller& controller_;

        /** Queue thread */
        std::jthread queue_thread_;
        std::atomic_bool queue_running_ {false};
    };

} // namespace constellation::controller
